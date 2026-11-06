# NEXT

Step 1 of PLAN.md: MIDI proof. With the user at the box, claim IF 2 (libusb or IOUSBLib),
read bulk IN 0x83 for ~10 s while a fader moves and Play is pressed.

Done = raw bytes captured and saved to docs/MIDI_CAPTURE.md, or a clear failure reason
(claim refused / no bytes → needs init command, search kernel bug 217084).
