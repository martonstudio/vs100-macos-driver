/*
 * vs100_midid — bridges the Cakewalk/Roland V-Studio 100's MIDI interface (interface 2,
 * bulk OUT 0x04 / IN 0x83, standard 4-byte USB-MIDI packets) to CoreMIDI: one virtual
 * source + destination pair per USB-MIDI cable.
 *   cable 0 -> "VS-100 MIDI"     (5-pin MIDI port)
 *   cable 1 -> "VS-100 Control"  (Mackie-style control surface, see docs/MIDI_CAPTURE.md)
 *
 * Runs as a launchd user agent; needs no privileges. It opens only interface 2
 * (USBInterfaceOpen), never the device, so it coexists with the HAL plug-in in coreaudiod,
 * which holds the device open and owns interfaces 0/1.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOMessage.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USB.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../driver/VS100_USB.h"
#include "../driver/VS100_Log.h"

#define RXBUF 512
#define NCABLES 2
#define SYXMAX 4096

static MIDIClientRef gClient; static MIDIEndpointRef gSource[NCABLES], gDest[NCABLES];
static const char *kNames[NCABLES] = { "VS-100 MIDI", "VS-100 Control" };   /* cable 0 = 5-pin port, cable 1 = control surface */
static IONotificationPortRef gNotify; static io_iterator_t gIfIter, gDevIter;
static IOUSBInterfaceInterface942 **gIf; static io_object_t gInterest; static CFRunLoopSourceRef gSrc;
static UInt8 gInPipe, gOutPipe; static uint8_t gRx[RXBUF];
static uint8_t gCin[16] = { 0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1 };
static uint64_t gRxPackets[NCABLES], gTxPackets[NCABLES];

/* ---------- USB -> CoreMIDI ---------- */
static uint8_t gRxSyx[NCABLES][SYXMAX]; static size_t gRxSyxN[NCABLES];
static void rx_complete(void *refcon, IOReturn result, void *arg0);
static void submit_read(void) {
    if (!gIf) return;
    IOReturn kr = (*gIf)->ReadPipeAsync(gIf, gInPipe, gRx, RXBUF, rx_complete, NULL);
    if (kr) LOGE("ReadPipeAsync failed 0x%x", kr);
}
static void emit(int cable, const uint8_t *d, size_t len) {
    Byte plbuf[SYXMAX + 64]; MIDIPacketList *pl = (MIDIPacketList *)plbuf; MIDIPacket *pk = MIDIPacketListInit(pl);
    pk = MIDIPacketListAdd(pl, sizeof plbuf, pk, 0, len, d);
    if (pk) MIDIReceived(gSource[cable], pl);
}
static void rx_complete(void *refcon, IOReturn result, void *arg0) {
    (void)refcon;
    if (result == kIOReturnAborted || result == kIOReturnNoDevice || result == kIOReturnNotResponding || !gIf) return;
    if (result == kIOReturnSuccess) {
        size_t n = (size_t)(uintptr_t)arg0;
        for (size_t i = 0; i + 4 <= n; i += 4) {
            int cable = gRx[i] >> 4; uint8_t cin = gRx[i] & 0x0F; uint8_t len = gCin[cin];
            if (!len || cable >= NCABLES) continue;
            gRxPackets[cable]++;
            if (cin >= 4 && cin <= 7) {                       /* SysEx: reassemble until F7 */
                for (uint8_t k = 0; k < len && gRxSyxN[cable] < SYXMAX; k++) gRxSyx[cable][gRxSyxN[cable]++] = gRx[i + 1 + k];
                if (cin != 4) { emit(cable, gRxSyx[cable], gRxSyxN[cable]); gRxSyxN[cable] = 0; }
            } else emit(cable, &gRx[i + 1], len);
        }
    }
    submit_read();
}

/* ---------- CoreMIDI -> USB ---------- */
typedef struct { uint8_t running; int inSysex; uint8_t syx[3]; int syxN; } enc_t;
static enc_t gEnc[NCABLES];
static void usb_send(const uint8_t *pkts, size_t n) {
    if (!gIf || !n) return;
    IOReturn kr = (*gIf)->WritePipe(gIf, gOutPipe, (void *)pkts, (UInt32)n);
    if (kr) LOGE("WritePipe failed 0x%x", kr);
}
static void encode_bytes(int cable, const uint8_t *b, size_t n) {
    enc_t *e = &gEnc[cable]; uint8_t cn = (uint8_t)(cable << 4);
    uint8_t out[512]; size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t c = b[i];
        if (c >= 0xF8) { out[o++] = cn | 0x0F; out[o++] = c; out[o++] = 0; out[o++] = 0; }         /* realtime */
        else if (e->inSysex) {
            e->syx[e->syxN++] = c;
            if (c == 0xF7) { out[o++] = cn | (uint8_t)(0x04 + e->syxN); memcpy(&out[o], e->syx, 3); o += 3; e->syxN = 0; e->inSysex = 0; memset(e->syx, 0, 3); }
            else if (e->syxN == 3) { out[o++] = cn | 0x04; memcpy(&out[o], e->syx, 3); o += 3; e->syxN = 0; memset(e->syx, 0, 3); }
        }
        else if (c == 0xF0) { e->inSysex = 1; e->syxN = 0; memset(e->syx, 0, 3); e->syx[e->syxN++] = c; }
        else if (c >= 0x80) {                                                                   /* status */
            uint8_t hi = c >> 4; size_t need = (hi == 0xC || hi == 0xD) ? 1 : 2;
            if (c == 0xF1 || c == 0xF3) need = 1; else if (c == 0xF2) need = 2; else if (c == 0xF6 || c == 0xF4 || c == 0xF5) need = 0;
            if (i + need >= n) break;
            uint8_t cin = c < 0xF0 ? hi : (need == 0 ? 0x5 : need == 1 ? 0x2 : 0x3);
            out[o++] = cn | cin; out[o++] = c; out[o++] = need >= 1 ? b[i + 1] : 0; out[o++] = need >= 2 ? b[i + 2] : 0;
            if (c < 0xF0) e->running = c;
            i += need;
        } else if (e->running) {                                                                /* running status */
            uint8_t hi = e->running >> 4; size_t need = (hi == 0xC || hi == 0xD) ? 1 : 2;
            if (i + need > n) break;
            out[o++] = cn | hi; out[o++] = e->running; out[o++] = b[i]; out[o++] = need == 2 ? b[i + 1] : 0;
            i += need - 1;
        }
        if (o + 8 > sizeof out) { usb_send(out, o); o = 0; }
    }
    if (o) { gTxPackets[cable] += o / 4; usb_send(out, o); }
}
static void dest_read(const MIDIPacketList *pl, void *rc, void *src) {
    (void)src; int cable = (int)(intptr_t)rc;
    const MIDIPacket *p = &pl->packet[0];
    for (UInt32 i = 0; i < pl->numPackets; i++) { encode_bytes(cable, p->data, p->length); p = MIDIPacketNext(p); }
}

/* ---------- interface attach / detach ---------- */
static void set_online(Boolean online) {
    for (int c = 0; c < NCABLES; c++) {
        MIDIObjectSetIntegerProperty(gSource[c], kMIDIPropertyOffline, !online);
        MIDIObjectSetIntegerProperty(gDest[c], kMIDIPropertyOffline, !online);
    }
}
static void detach(void) {
    if (!gIf) return;
    LOG("MIDI interface detached");
    (*gIf)->AbortPipe(gIf, gInPipe);
    if (gSrc) { CFRunLoopRemoveSource(CFRunLoopGetCurrent(), gSrc, kCFRunLoopDefaultMode); gSrc = NULL; }
    if (gInterest) { IOObjectRelease(gInterest); gInterest = 0; }
    (*gIf)->USBInterfaceClose(gIf); (*gIf)->Release(gIf); gIf = NULL;
    set_online(false);
}
static void interest(void *rc, io_service_t s, natural_t msg, void *arg) { (void)rc;(void)s;(void)arg; if (msg == kIOMessageServiceIsTerminated) detach(); }

static int prop_int(io_service_t s, CFStringRef key) {
    CFTypeRef v = IORegistryEntryCreateCFProperty(s, key, kCFAllocatorDefault, 0); int r = -1;
    if (v && CFGetTypeID(v) == CFNumberGetTypeID()) CFNumberGetValue(v, kCFNumberIntType, &r);
    if (v) CFRelease(v);
    return r;
}
static void attach(io_service_t ifs) {
    /* Property matching on IOUSBHostInterface does not work here (every key matches nothing),
     * so the notification matches every interface and we filter on the registry properties. */
    if (prop_int(ifs, CFSTR("idVendor")) != VS100_VID || prop_int(ifs, CFSTR("idProduct")) != VS100_PID_ADVANCED || prop_int(ifs, CFSTR("bInterfaceNumber")) != 2) return;
    if (gIf) return;
    IOCFPlugInInterface **plug = NULL; SInt32 score;
    IOReturn pr = IOCreatePlugInInterfaceForService(ifs, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &plug, &score);
    if (pr || !plug) { LOGE("IOCreatePlugInInterfaceForService(interface) failed 0x%x", pr); return; }
    IOUSBInterfaceInterface942 **ifc = NULL; (*plug)->QueryInterface(plug, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID942), (LPVOID *)&ifc); (*plug)->Release(plug);
    if (!ifc) return;
    if ((*ifc)->USBInterfaceOpen(ifc)) { LOGE("USBInterfaceOpen failed"); (*ifc)->Release(ifc); return; }
    /* altsetting 0 (bulk 0x04 / 0x83) is the one verified with tools/vs100_midi_probe.py */
    UInt8 neps = 0; (*ifc)->GetNumEndpoints(ifc, &neps); gInPipe = gOutPipe = 0;
    for (UInt8 p = 1; p <= neps; p++) { UInt8 dir, num, tt, ivl; UInt16 mps; if ((*ifc)->GetPipeProperties(ifc, p, &dir, &num, &tt, &mps, &ivl)) continue;
        if (tt != kUSBBulk && tt != kUSBInterrupt) continue; if (dir == kUSBIn && !gInPipe) gInPipe = p; if (dir == kUSBOut && !gOutPipe) gOutPipe = p; }
    if (!gInPipe || !gOutPipe) { LOGE("MIDI pipes not found"); (*ifc)->USBInterfaceClose(ifc); (*ifc)->Release(ifc); return; }
    if ((*ifc)->CreateInterfaceAsyncEventSource(ifc, &gSrc) == 0) CFRunLoopAddSource(CFRunLoopGetCurrent(), gSrc, kCFRunLoopDefaultMode);
    IOServiceAddInterestNotification(gNotify, ifs, kIOGeneralInterest, interest, NULL, &gInterest);
    gIf = ifc; memset(gEnc, 0, sizeof gEnc); memset(gRxSyxN, 0, sizeof gRxSyxN);
    set_online(true);
    submit_read();
    LOG("MIDI interface attached (in pipe %u, out pipe %u)", gInPipe, gOutPipe);
}
static void ifaces_added(void *rc, io_iterator_t it) { (void)rc; io_service_t s; while ((s = IOIteratorNext(it))) { attach(s); IOObjectRelease(s); } }

/* If nothing has configured the device yet (HAL plug-in absent), do it so the
 * interfaces appear. Harmless if the device is already configured or busy. */
static void devices_added(void *rc, io_iterator_t it) {
    (void)rc; io_service_t s;
    while ((s = IOIteratorNext(it))) {
        IOCFPlugInInterface **plug = NULL; SInt32 score;
        if (IOCreatePlugInInterfaceForService(s, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plug, &score) == 0 && plug) {
            IOUSBDeviceInterface942 **dev = NULL; (*plug)->QueryInterface(plug, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID942), (LPVOID *)&dev); (*plug)->Release(plug);
            if (dev) { UInt8 c = 0; (*dev)->GetConfiguration(dev, &c);
                if (c == 0 && (*dev)->USBDeviceOpen(dev) == 0) { (*dev)->SetConfiguration(dev, 1); (*dev)->USBDeviceClose(dev); }
                (*dev)->Release(dev); }
        }
        IOObjectRelease(s);
    }
}

static void midi_notify(const MIDINotification *m, void *rc) { (void)m; (void)rc; }

int main(void) {
    OSStatus st = MIDIClientCreate(CFSTR("VS-100 Bridge"), midi_notify, NULL, &gClient);
    if (st) { LOGE("MIDIClientCreate failed %d", (int)st); return 1; }
    for (int c = 0; c < NCABLES; c++) {
        CFStringRef nm = CFStringCreateWithCString(NULL, kNames[c], kCFStringEncodingUTF8);
        OSStatus s1 = MIDISourceCreate(gClient, nm, &gSource[c]); if (s1) LOGE("MIDISourceCreate %d failed %d", c, (int)s1);
        OSStatus s2 = MIDIDestinationCreate(gClient, nm, dest_read, (void *)(intptr_t)c, &gDest[c]); if (s2) LOGE("MIDIDestinationCreate %d failed %d", c, (int)s2);
        CFRelease(nm);
        MIDIObjectSetIntegerProperty(gSource[c], kMIDIPropertyUniqueID, 0x56530010 + c);
        MIDIObjectSetIntegerProperty(gDest[c], kMIDIPropertyUniqueID, 0x56530020 + c);
        MIDIObjectSetStringProperty(gSource[c], kMIDIPropertyManufacturer, CFSTR("Roland / Cakewalk"));
        MIDIObjectSetStringProperty(gDest[c], kMIDIPropertyManufacturer, CFSTR("Roland / Cakewalk"));
        MIDIObjectSetStringProperty(gSource[c], kMIDIPropertyModel, CFSTR("VS-100"));
        MIDIObjectSetStringProperty(gDest[c], kMIDIPropertyModel, CFSTR("VS-100"));
    }
    set_online(false);

    gNotify = IONotificationPortCreate(kIOMainPortDefault);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(gNotify), kCFRunLoopDefaultMode);
    int vid = VS100_VID, pid = VS100_PID_ADVANCED;   /* MIDI = interface 2 (FF/01/03) */
    CFMutableDictionaryRef m = IOServiceMatching("IOUSBHostInterface");
    CFNumberRef v = CFNumberCreate(NULL, kCFNumberIntType, &vid), p = CFNumberCreate(NULL, kCFNumberIntType, &pid);
    IOServiceAddMatchingNotification(gNotify, kIOFirstMatchNotification, m, ifaces_added, NULL, &gIfIter);
    ifaces_added(NULL, gIfIter);
    CFMutableDictionaryRef dm = IOServiceMatching(kIOUSBDeviceClassName);
    CFDictionarySetValue(dm, CFSTR(kUSBVendorID), v); CFDictionarySetValue(dm, CFSTR(kUSBProductID), p);
    IOServiceAddMatchingNotification(gNotify, kIOFirstMatchNotification, dm, devices_added, NULL, &gDevIter);
    devices_added(NULL, gDevIter);
    CFRelease(v); CFRelease(p);

    LOG("vs100_midid running");
    CFRunLoopRun();
    return 0;
}
