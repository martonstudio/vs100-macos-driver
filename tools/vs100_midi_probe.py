# Listen on the VS-100 MIDI/control interface (IF 2, bulk IN 0x83) and print raw packets.
# Usage: uv run --with pyusb python -I tools/vs100_midi_probe.py [seconds]
import sys, time, usb.core, usb.util, usb.backend.libusb1 as L
be = L.get_backend(find_library=lambda x: "/opt/homebrew/lib/libusb-1.0.dylib")
d = usb.core.find(idVendor=0x0582, idProduct=0x00EB, backend=be)
if d is None: sys.exit("VS-100 not found")
try: d.get_active_configuration()
except usb.core.USBError: d.set_configuration(1)
if d.get_active_configuration() is None: d.set_configuration(1)
usb.util.claim_interface(d, 2)
print("claimed IF 2, listening on 0x83", flush=True)
end = time.time() + float(sys.argv[1] if len(sys.argv) > 1 else 10)
n = 0
while time.time() < end:
    try:
        data = d.read(0x83, 512, timeout=200)
    except usb.core.USBTimeoutError:
        continue
    n += 1
    b = bytes(data)
    print(f"{time.time():.3f} len={len(b):3d} " + " ".join(b[i:i+4].hex() for i in range(0, len(b), 4)), flush=True)
usb.util.release_interface(d, 2)
print(f"done, {n} packets")
