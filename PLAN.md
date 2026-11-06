# PLAN — VS-100 on Apple Silicon macOS

Finish line: the V-Studio 100 appears in Audio MIDI Setup as a 6-out / 10-in 48 kHz
CoreAudio device that plays and records glitch-free in a DAW for 10+ minutes, and its
faders/transport show up as a CoreMIDI device. User-space only.

## Steps
1. MIDI proof: claim IF 2 from user space, read bulk 0x83 while moving a fader. Proves
   macOS lets user space own the device and shows the control-surface byte format.
2. MIDI bridge: adapt `midi/ua4fx_midid.c` to VID/PID 0582:00EB, endpoints 0x04/0x83.
   Done = faders visible in MIDI Monitor.
3. Engine port in `driver/UA4FX_USB.c`, tested with `tools/ua4fx_test` before any HAL work:
   high-speed microframe scheduling, 4-byte samples, 6/10 channels, implicit feedback
   (out packet sizes follow captured packet sizes). Done = 440 Hz tone on outputs 1/2 and
   clean input counts for 60 s.
4. HAL plug-in: channel counts and names in `driver/UA4FX_Driver.c`, verify with
   `tools/hal_harness`, then install to coreaudiod.
5. Rename product strings/bundle IDs from UA4FX to VS100 (separate commit, no behaviour change).

## Constraints
- No kexts, DriverKit entitlements, SIP changes, or firmware flashing. Firmware route is
  closed: no public toolchain, brick risk, and it would not solve implicit feedback or MIDI.
- Base code: mlodyrafael/ua4fx-macos-driver @ e8b9033 (MIT). Keep its LICENSE and credit.
- Work from USB observation and Linux sources, never by disassembling Roland's driver.
- Descriptor facts live in `docs/DESCRIPTORS.md`; read it rather than re-probing.
- `ref/` holds read-only upstream clones and is not committed.
