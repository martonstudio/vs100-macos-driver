# VS-100 USB descriptors

Read from the live unit (serial S35847D) on mbpro, macOS 26.4, 2026-11-07, via libusb
GET_DESCRIPTOR. Firmware bcdDevice 0x0100.

Device: VID 0x0582 PID 0x00EB "Roland / VS-100 Audio/MIDI", class FF/00/FF,
USB 2.0 **high speed** (480 Mbit/s, 125 µs microframes), 1 configuration, 3 interfaces.

| IF | alt | class | endpoint | type | maxpkt | format |
|---|---|---|---|---|---|---|
| 0 | 1 | FF/01/02 | 0x02 OUT | isoc **async** (0x05), interval 1 | 168 | 6 ch, 24-bit in 4-byte slots, 48000 Hz only |
| 1 | 1 | FF/01/02 | 0x81 IN | isoc async, **implicit feedback** (0x25), interval 1 | 280 | 10 ch, 24-bit in 4-byte slots, 48000 Hz only |
| 2 | 0 | FF/01/03 | 0x04 OUT / 0x83 IN | bulk | 512 | MIDI |
| 2 | 1 | FF/01/03 | 0x04 OUT / 0x83 IN | interrupt | 512 | MIDI |

Alt 0 on IF 0/1 = zero bandwidth. maxpkt 168 = 7 frames x 24 bytes (6 nominal per
microframe + 1); 280 = 7 x 40.

IF 2 carries a Roland vendor descriptor `06 24 F1 02 01 01` (same shape as UA-4FX's MIDI
interface).

## Differences from UA-4FX (the code base this repo starts from)

| | UA-4FX | VS-100 |
|---|---|---|
| Bus speed | full speed, 1 ms frames | high speed, 125 µs microframes |
| Out endpoint | 0x01 adaptive | 0x02 async, clocked by capture (implicit feedback) |
| In endpoint | 0x82 | 0x81 |
| Sample format | 24-bit in 3 bytes, stereo | 24-bit in 4 bytes, 6 out / 10 in |
| Rates | 44.1/48/96 via rear switch | 48 kHz only (per descriptor) |
| MIDI | bulk 0x03 / 0x84 | bulk 0x04 / 0x83 |

## Raw configuration descriptor (167 bytes)

```
0902a700030100c0000904000000ff0102000624f10101000904000101ff010200072401010001010b2402010604180180bb0007050205a80001072501000000000904010000ff0102000904010101ff010200072401010001010b2402010a04180180bb0007058125180101072501000000000904020002ff0103000624f102010107050402000201070583020002010904020102ff0103000705040300020107058303000201
```
