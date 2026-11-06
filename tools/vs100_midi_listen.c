/*
 * vs100_midi_listen — lists the VS-100 CoreMIDI endpoints and prints MIDI received from
 * them (default 15 s). Move a fader / press a transport button while it runs.
 *   vs100_midi_listen [seconds] [--send]
 * --send transmits a Note On/Off pair (note 60, ch 1) to the "VS-100 MIDI" destination
 * (the 5-pin MIDI OUT) to exercise the host->device path; nothing is sent to the control surface.
 */
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long gCount[16];
static void name_of(MIDIObjectRef o, char *b, size_t n) {
    CFStringRef s = NULL; b[0] = 0;
    MIDIObjectGetStringProperty(o, kMIDIPropertyName, &s);
    if (s) { CFStringGetCString(s, b, n, kCFStringEncodingUTF8); CFRelease(s); }
}
static void rd(const MIDIPacketList *pl, void *rc, void *src) {
    int idx = (int)(intptr_t)rc; const MIDIPacket *p = &pl->packet[0];
    for (UInt32 i = 0; i < pl->numPackets; i++) {
        printf("src %d:", idx); for (UInt16 k = 0; k < p->length; k++) printf(" %02x", p->data[k]); printf("\n"); fflush(stdout);
        gCount[idx]++; p = MIDIPacketNext(p);
    }
}
int main(int argc, char **argv) {
    int secs = 15, send = 0;
    for (int i = 1; i < argc; i++) { if (!strcmp(argv[i], "--send")) send = 1; else secs = atoi(argv[i]); }
    MIDIClientRef cl; MIDIClientCreate(CFSTR("vs100_midi_listen"), NULL, NULL, &cl);
    MIDIPortRef in, out; MIDIInputPortCreate(cl, CFSTR("in"), rd, NULL, &in); MIDIOutputPortCreate(cl, CFSTR("out"), &out);
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.5, false);   /* let CoreMIDI populate the endpoint list */
    int found = 0; char nm[128]; printf("CoreMIDI: %lu sources, %lu destinations\n", (unsigned long)MIDIGetNumberOfSources(), (unsigned long)MIDIGetNumberOfDestinations());
    ItemCount ns = MIDIGetNumberOfSources(), nd = MIDIGetNumberOfDestinations();
    MIDIEndpointRef dest0 = 0;
    for (ItemCount i = 0; i < ns; i++) {
        MIDIEndpointRef e = MIDIGetSource(i); name_of(e, nm, sizeof nm); if (strncmp(nm, "VS-100", 6)) continue;
        SInt32 off = 0; MIDIObjectGetIntegerProperty(e, kMIDIPropertyOffline, &off);
        printf("source      \"%s\" %s\n", nm, off ? "(offline)" : "(online)");
        MIDIPortConnectSource(in, e, (void *)(intptr_t)found); printf("  listening as src %d\n", found); found++;
    }
    for (ItemCount i = 0; i < nd; i++) {
        MIDIEndpointRef e = MIDIGetDestination(i); name_of(e, nm, sizeof nm); if (strncmp(nm, "VS-100", 6)) continue;
        SInt32 off = 0; MIDIObjectGetIntegerProperty(e, kMIDIPropertyOffline, &off);
        printf("destination \"%s\" %s\n", nm, off ? "(offline)" : "(online)"); if (!strcmp(nm, "VS-100 MIDI")) dest0 = e;
    }
    if (!found) { for (ItemCount i = 0; i < ns; i++) { name_of(MIDIGetSource(i), nm, sizeof nm); printf("  (other source: %s)\n", nm); }
        printf("No VS-100 MIDI endpoints. Is the bridge running? (launchctl print gui/$(id -u)/com.vs100.midid)\n"); return 1; }
    if (send) {
        if (!dest0) { printf("no \"VS-100 MIDI\" destination\n"); return 1; }
        Byte b[64]; MIDIPacketList *pl = (MIDIPacketList *)b; MIDIPacket *p = MIDIPacketListInit(pl);
        Byte on[3] = {0x90, 60, 100}, off[3] = {0x80, 60, 0};
        p = MIDIPacketListAdd(pl, sizeof b, p, 0, 3, on); MIDIPacketListAdd(pl, sizeof b, p, 0, 3, off);
        OSStatus st = MIDISend(out, dest0, pl); printf("MIDISend to \"VS-100 MIDI\": status %d\n", (int)st);
    }
    printf("listening %d s: move a fader / press a button...\n", secs); fflush(stdout);
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, secs, false);
    unsigned long tot = 0; for (int i = 0; i < found; i++) { printf("src %d: %lu message(s)\n", i, gCount[i]); tot += gCount[i]; }
    if (tot) printf("RESULT: received %lu messages\n", tot); else printf("RESULT: no MIDI received\n");
    return 0;
}
