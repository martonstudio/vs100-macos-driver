# Starter prompts

Paste one per session. Each assumes README.md is in the project folder.

## 1. Research consolidation
Read README.md. Fetch kernel bug 217084 (all comments and attachments), the
LinuxMusicians VS-100 thread, and the MMUSBAUDIO project. Write docs/PROTOCOL.md with
everything known about the VS-100 USB interface: VID/PID, interfaces, alt settings,
endpoints, sample rates/formats, channel counts, implicit feedback, init sequences,
and which kernel patches (if any) were merged. Cite every claim.

## 2. Device identification
Here is `lsusb -v` / `ioreg -p IOUSB -l` output from my VS-100: <paste>. Parse it into
docs/DESCRIPTORS.md and reconcile with docs/PROTOCOL.md. List what's still unknown.

## 3. MIDI / control surface
Clone MultiRolandDriver. Check whether the VS-100 PID is supported. If not, add it
following the existing device-table pattern, build a universal binary, and give me
install + test steps (MIDI Monitor). Note which ports carry faders/transport.

## 4. Audio driver port
Clone ua4fx-macos-driver. Read its architecture and write docs/PORTING.md mapping
each UA-4FX assumption (endpoints, channels, rates, feedback, control requests) to the
VS-100 values from docs/PROTOCOL.md. Then implement the port behind a device table so
both devices remain supported. Start with 2-in/2-out at 44.1/48 kHz.

## 5. Debugging on hardware
The driver loads but <symptom: no sound / glitches / device not found>. Here are logs:
<paste>. Diagnose against docs/PROTOCOL.md; propose the smallest fix and a test.

## 6. Upstream / share
Prepare a PR to MultiRolandDriver (MIDI) and to ua4fx-macos-driver (or a fork) with the
VS-100 support, plus a short forum post for LinuxMusicians/VI-Control.
