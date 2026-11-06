# Cakewalk Roland Driver

Goal: get the Cakewalk/Roland V-Studio 100 (VS-100) working on Apple Silicon macOS
(audio + MIDI/control surface), without kexts, SIP changes or DriverKit entitlements.

## Status of the world (researched 2026-10-15)
- Official drivers stop at OS X 10.11 (kext-based, won't load on Apple Silicon).
  https://www.roland.com/us/support/by_product/v-studio_100/updates_drivers/
  https://legacy.cakewalk.com/Support/Download/HwDrivers/V-Studio%20100
- Reflashing to class-compliant: not realistic (proprietary Roland firmware, no toolchain).
- No existing VS-100 macOS driver found.

## Prior art to build on
- ua4fx-macos-driver — user-space CoreAudio HAL plug-in over IOUSBLib for the Roland UA-4FX
  on Apple Silicon, plus CoreMIDI bridge. No kext/DriverKit/SIP. **Primary template.**
  https://github.com/mlodyrafael/ua4fx-macos-driver
- MultiRolandDriver — CoreMIDI plug-in for many Roland USB MIDI devices with abandoned
  drivers, universal binary. MIDI only. Check if VS-100 is listed; else add its PID.
  https://github.com/lnswlrd/MultiRolandDriver
- Linux kernel bug 217084 — VS-100 broken in snd-usb-audio since 5.11; contains a
  "VS-100-quirk-simple-proposal" and test patches from Takashi Iwai. Protocol spec source.
  https://bugzilla.kernel.org/show_bug.cgi?id=217084
- LinuxMusicians thread "[Solved] Cakewalk v-studio 100 on linux" (+ MMUSBAUDIO mention).
  https://linuxmusicians.com/viewtopic.php?t=23883&start=75

## Install / uninstall / test

Requires macOS 13+ on Apple silicon. The pkg is ad-hoc signed only, so Gatekeeper blocks a
double-click: use right-click > Open, or install from Terminal.

```bash
make pkg                       # -> build/VS100-Driver-0.1.0.pkg
make install                   # = sudo installer -pkg build/VS100-Driver-0.1.0.pkg -target /
```

The pkg installs the HAL plug-in `/Library/Audio/Plug-Ins/HAL/VS100.driver`, the MIDI bridge
`/Library/Application Support/VS100/vs100_midid` with LaunchAgent `/Library/LaunchAgents/com.vs100.midid.plist`,
`/Applications/VS100 Control.app` and the uninstaller, removes any old `UA4FX.driver` / `com.ua4fx.*` agent,
and restarts coreaudiod (Mac audio cuts for a moment).

Uninstall (removes every file, unloads the agent, forgets the receipt, restarts coreaudiod):
`sudo "/Library/Application Support/VS100/uninstall.sh"`, or double-click
`/Applications/Uninstall VS-100 Driver.command`, or `make uninstall`.

Tests (with the VS-100 connected; inputs 9/10 are a device-internal loopback of outputs 1/2):
- `system_profiler SPAudioDataType | grep -A6 "V-Studio"` : 10 in / 6 out, 48000 Hz
- `./build/rtl_test 64 9 [seconds]` : output->input latency and click-to-click jitter via CoreAudio
- `scripts/check-midi.sh` : lists the MIDI endpoints ("VS-100 MIDI", "VS-100 Control") and prints
  15 s of received MIDI; move a fader meanwhile
- `./build/vs100_test 60` : engine exerciser, needs the device free (uninstall the plug-in first)
- logs: `/usr/bin/log show --last 2m --predicate 'subsystem == "com.vs100.driver"'`

Credit: forked from mlodyrafael/ua4fx-macos-driver @ e8b9033 (MIT, see LICENSE and
docs/UPSTREAM_UA4FX_README.md for the architecture).

## Plan
See PLAN.md (steps) and NEXT.md (current step). Descriptors: docs/DESCRIPTORS.md.
Control-surface bytes: docs/MIDI_CAPTURE.md. Loopback jitter analysis: docs/LOOPBACK_JITTER.md.
