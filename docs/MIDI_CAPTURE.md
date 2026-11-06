# VS-100 MIDI / control surface (IF 2, bulk IN 0x83)

Captured 2026-11-07 with tools/vs100_midi_probe.py. No init command needed: packets
arrive as soon as IF 2 is claimed after SET_CONFIGURATION 1.

Framing: standard USB-MIDI 1.0, 4-byte events, byte 0 = cable (high nibble) + CIN.
All control-surface traffic is on **cable 1**. Cable 0 is presumably the MIDI IN socket (untested).

Protocol is Mackie Control style:

| Bytes | Meaning | Control |
|---|---|---|
| `19 90 68 7f` / `19 90 68 00` | Note On ch1 note 104 on/off | fader touch |
| `1e e0 LL MM` | Pitch bend ch1, 14-bit | fader position |
| `19 90 5b..5f 7f/00` | Note On notes 91–95 | REW / FF / STOP / PLAY / REC (Mackie numbering) |
| `19 91 21 7f/00`, `19 91 22 7f/00` | Note On ch2 notes 33/34 | unidentified buttons |
| `1b b0 10 41` / `1b b0 10 01` | CC 16, relative | encoder: 0x41 = one step left, 0x01 = one step right |

Not yet captured: other faders/encoders, host-to-device messages (LEDs, motor fader if any).
