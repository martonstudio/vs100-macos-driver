# PLAN — VS-100 on Apple Silicon macOS

Finish line: the V-Studio 100 appears as a 6-out / 10-in 48 kHz CoreAudio device that plays and
records glitch-free in a DAW for 10+ minutes, and its faders/transport arrive in a DAW via CoreMIDI.
User-space only, installed by `build/VS100-Driver-<version>.pkg`.

## Steps
1. Hands test of MIDI receive: `scripts/check-midi.sh` while moving faders / pressing transport
   (cable 1 expected; cable 0 needs a MIDI cable from the 5-pin OUT to a source).
2. DAW soak: 10+ minutes play + record in a DAW at 48 kHz, buffer 64..512, watch
   `log show --predicate 'subsystem == "com.vs100.driver"'` for rxErr/resyncs/snaps.
3. Control surface: map Mackie-style messages (docs/MIDI_CAPTURE.md) in the DAW; capture remaining
   faders/encoders and host-to-device messages (LEDs / motor faders) if they matter.
4. Gain/mixer: the VS-100 has hardware mixer controls on the control-request path; not yet analysed.

## Constraints
- No kexts, DriverKit entitlements, SIP changes, or firmware flashing. Firmware route is
  closed: no public toolchain, brick risk, and it would not solve implicit feedback or MIDI.
- Base code: mlodyrafael/ua4fx-macos-driver @ e8b9033 (MIT). Keep its LICENSE and credit.
- Roland firmware and drivers may be analysed and disassembled (owner's right to repair, EU/AU). Vendor files stay out of git.
- Descriptor facts live in `docs/DESCRIPTORS.md`; read it rather than re-probing.
- `ref/` holds read-only upstream clones and is not committed.
- Never run `build/vs100_test` while the plug-in is installed: coreaudiod holds the USB device (USBDeviceOpen fails).
- The MIDI bridge must open only interface 2 (never the device) so it can coexist with the HAL plug-in.
- IOKit property matching on IOUSBHostInterface matches nothing on this macOS: match the class, filter on registry properties.
- Measure loopback onset by threshold crossing, never by peak position (the converter filter rings; docs/LOOPBACK_JITTER.md).
- Staging for pkgbuild must be outside Dropbox (Dropbox xattrs get packed as ._ files).
