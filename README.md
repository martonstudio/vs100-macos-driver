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

## Plan
See PLAN.md (steps) and NEXT.md (current step). Descriptors: docs/DESCRIPTORS.md.

Note: Cakewalk's EULA forbids reverse engineering *their software*; work from the USB
protocol, Linux sources and your own captures, not by disassembling Roland's driver.
