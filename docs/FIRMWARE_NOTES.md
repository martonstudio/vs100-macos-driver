# VS-100 system firmware v1.50: structure notes

Source: Roland's public update `vs100_sys_v150.zip` (files `UsbUp/_00001.mid` … `_00008.mid`,
build date Sep 7 2010). Analysed offline on 2026-11-07; nothing was sent to the unit.
Vendor files and decoded images stay outside the repo.

Labels: **[V]** verified (ran or observed in this analysis), **[I]** inferred (reasoning or
prior knowledge, not checked against the binary).

## Summary

- CPU: **Analog Devices Blackfin** (ADSP-BF5xx family). Exact part not determined. [V for
  Blackfin, see evidence below]
- Flash: 1 MB, mapped at CPU address **0x20000000** (async memory bank 0). Booter executes
  in place from there. [V: code self-references]
- Compression: **LZSS, Okumura LZSS.C parameters** (4096-byte ring, max match 18, min match
  3, flag byte LSB-first, 1 = literal). Ring pre-filled with 0x00. Decompressor at CPU
  0x20002FB0 in the loader. [V]
- Updater and application are both LZSS-compressed; only the booter and loader are plain
  code (16 KB, not ~32 KB). Both decompress to SDRAM at **0x00000100** and run there. [V]

## 1. SysEx transport

SMF stores each SysEx as `F0 <len> <bytes…> F7`. As SysEx: `F0 41 <dev> 00 69 12 <addr:5>
<body> <cs> F7`. Checksum: Roland (sum of addr + body + cs) & 0x7F == 0, all messages pass. [V]

**Address** = 5 bytes of 7 bits each, big-endian, giving a flat byte offset into the flash.
Example: `00 00 03 7F 70` = 0xFFF0. Address = flash offset; CPU address = 0x20000000 +
offset. [V for offset: every data byte lands once, no overlaps, checksums match. CPU base
inferred from code, see 3.]

**Body** of every message uses 7-to-8 packing (1 high-bit byte + 7 bytes, bit i → byte i). [V:
the per-block checksums below only match with this mapping]

Three message kinds, by `<dev>` byte: [V]

| dev | length | role |
|---|---|---|
| 0x00 | 20 bytes | Block header, sent before each 64 KB block. Body unpacks to 6 bytes: 16-bit big-endian sum of all 32 768 big-endian words in the block, then length `00 01 00 00` (65 536). |
| 0x01 | 77 bytes | Data: 64 packed → 56 raw bytes. 1170 per block. |
| 0x01 | 32 bytes | Last 16 bytes of a block (1170 × 56 + 16 = 65 536). |
| 0x02 | 20 bytes | Identical to the dev 0x00 message, sent after the block. |

Meaning of dev 0/1/2 as "announce/erase, write, verify" is **[I]**.

Each file carries two 64 KB blocks (block = top address byte / 4): [V]

| File | Blocks (flash offset) | Data sent |
|---|---|---|
| _00001 | 0x00000, 0x10000 | yes, yes |
| _00002 | 0x20000, 0x30000 | yes, yes |
| _00003 | 0x40000, 0x50000 | yes, yes |
| _00004 | 0x60000, 0x70000 | yes, yes |
| _00005 | 0x80000, 0x90000 | yes, header only |
| _00006 | 0xA0000, 0xB0000 | header only |
| _00007 | 0xC0000, 0xD0000 | header only |
| _00008 | 0xE0000, 0xF0000 | header only, yes |

"Header only" blocks carry checksum 0x8000 = a block of all 0xFF, so they are erased and
left blank. [V for the checksum value; "erased" is I]. Block 0xF0000 is all 0xFF except
its last 4 bytes (`D4 01 66 10`).

The earlier "decode in file order → 655 200 bytes" view dropped the 16-byte tail of each
block and put block 0xF0000 directly after 0x80000. Placing data by address gives a 1 MB
image. [V]

`end.mid` holds one different message (`F0 41 10 45 00 12 7F 01 …`, another model ID).
Not analysed.

## 2. Flash layout

| Region | Flash offset | CPU address | Content |
|---|---|---|---|
| booter | 0x00000–0x00FFF | 0x20000000 | Plain code 0x000–0x63B. ID string at 0xF00. `00 01 FF FF` at 0xFF8, trailer `D4 01 54 A5` at 0xFFC. |
| loader | 0x01000–0x03FFF | 0x20001000 | Plain code 0x1000–0x243F and 0x2CDC–0x308B, data table 0x2440–0x2CDB. ID string at 0x3F00. `00 01 FF FF` at 0x3FF8, trailer `D4 01 1A 9F` at 0x3FFC. |
| updater | 0x04000–0x1FFFF | 0x20004000 | `FF FF FF FF`, ID string at 0x4004, `02 01 FF FF` at 0x40F8, uncompressed size (BE32) `00 02 7D 20` at 0x4100, LZSS stream 0x4104–0x1E1C0, trailer `D4 01 DB 44` at 0x1FFFC. |
| application | 0x20000–0xFFFFF | 0x20020000 | `FF FF FF FF`, ID string at 0x20004, `32 01 FF FF` at 0x200F8, size `00 0C 1E EC` at 0x20100, LZSS stream 0x20104–0x8E40E, erased 0x8E40F–0xFFFFB, trailer `D4 01 66 10` at 0xFFFFC. |

Byte positions are [V]. Region boundaries: booter→loader [V] (booter jumps to
0x20001000); the others follow the ID strings and trailers [I]. The application owning the
trailer at 0xFFFFC is [I]: it is the only trailer after 0x20000, and block 0xF0000 is sent
only to carry it.

ID strings: `EDIROL VS100 {booter|loader|updater|application} program / Copyright Roland
Corp. / Build date <Sep  7 2010/10:11:13 … 10:21:58 >`. [V]

Not decoded: the `D4 01 xxxx` trailer checksum (byte sum, 16-bit LE/BE word sums, and six
common CRC-16 variants over each region all failed to match), and the meaning of the
`00 01` / `02 01` / `32 01` bytes at region+0xF8.

## 3. CPU: Blackfin

Tool: Homebrew GNU binutils `objdump -b binary -m bfin`.

Scoring the booter (0x0–0xF00) against other architectures in the same tool: V850 639
invalid, V850E 15 invalid but nonsensical code (`or r8,r28` / `and r8,r28` repeated),
MN10300 11 (byte-granular encoding, rarely fails, low count proves little), M68K 1320,
RX 1184. Blackfin reads as coherent code. [V]

Independent check with `cpu_rec` (airbus-seclab, statistical classifier): flash
0x0000–0x3FFF (booter + loader) → **Blackfin**; 0x4000–0x1FFFF (updater, compressed) →
Blackfin; 0x20000–0x8FFFF (application, compressed) → 6502. The last result is
meaningless, since compressed data has no instruction statistics. [V]

Invalid-instruction counts as Blackfin: booter code 3 of 631; loader code: all 505
invalid entries fall in the 0x2440–0x2CD4 data table; first 16 KB of each decompressed
image 0. [V]

Booter at offset 0 (= 0x20000000) is a textbook Blackfin startup: [V for the listing,
register meanings from training I]

```
0x000  R0 = 0; LC0 = LC1 = L0..L3 = R0      ; clear loop/circular-buffer regs
0x00E  P0 = 0xFFB01000; SP = P0             ; stack at top of L1 scratchpad
0x018  CALL 0x200002FC
0x02A  P0 = 0xFFE02008                      ; core event vector table (EVT2)
0x034…0x0B6  store EVT2..EVT14 = 0, EVT15 = 0x200000D0
0x0B8  RETI = 0x200000DC; RAISE 15; STI 0x801F; RTI   ; drop to IVG15
0x0D0  CALL 0x200000EC; JUMP 0x20001000     ; init, then loader
0x0EC  writes 0xFFC00A10/0xFFC00A14          ; EBIU SDRAM controller setup
```

Load address 0x20000000 is [V] by consistency: the absolute targets 0x200000D0,
0x200002FC and 0x20001000 all land on valid code at offsets 0xD0, 0x2FC and 0x1000.
Blackfin has no reset-vector table in flash; the boot ROM in "bypass" mode starts
executing at 0x20000000 [I, training].

The loader repeats the same pattern (EVT15 = 0x200010EE), calls its main at 0x20001680,
then sets `P0 = 0x00000100` and `JUMP (P0)` at 0x20001102 into SDRAM. [V]

Chip variant: the application uses the BF52x on-chip USB register block (0xFFC038xx)
only 3 times, and the loader drives a device on async bank 3 (0x20300000). The USB
interface is probably an external controller. Exact Blackfin part: undetermined. [I]

## 4. Decompressor (loader, CPU 0x20002FB0, flash 0x2FB0–0x3089)

Behaviour read from the disassembly: [V]

- Inputs: R0 = source pointer (→ P1); R2 = output length (kept at [SP+0x24]). Output
  pointer is hard-wired: P4 = 0x00000100; end = 0x100 + length.
- Ring buffer: 4096 bytes at 0xFF904000 (L1 data SRAM [I]). First 0xFEE bytes zeroed
  (1019 × 32-bit + one 16-bit store). Write index r = 0xFEE, mask 0xFFF.
- Flag byte: when the sentinel bit 8 runs out, read a byte and OR with 0xFF00. Shift right
  each item; bit 0 = 1 → literal, 0 → reference.
- Literal: copy one byte to output and ring[r], r = (r+1) & 0xFFF.
- Reference: bytes b0 b1. offset = b0 | ((b1 & 0xF0) << 4), length = (b1 & 0x0F) + 3.
  Copy from ring[(offset+k) & 0xFFF], writing each byte to output and ring.
- Stops when the output pointer reaches the end, then returns.

That is Haruhiko Okumura's 1989 LZSS.C (N = 4096, F = 18, THRESHOLD = 2), except the ring is
pre-filled with 0x00 instead of spaces. [I for the attribution, V for the parameters]

The routine's caller was not located: there is no direct CALL to 0x20002FB0 and no stored
pointer to it in the image. It is probably reached via a computed address. [I]

**Python reimplementation, run on both payloads** [V]:

```python
def lzss(src, pos, outlen):
    ring = bytearray(4096); r = 0xFEE; out = bytearray(); flags = 0
    while len(out) < outlen:
        flags >>= 1
        if not flags & 0x100: flags = src[pos] | 0xFF00; pos += 1
        if flags & 1:
            c = src[pos]; pos += 1; out.append(c); ring[r] = c; r = (r + 1) & 0xFFF
        else:
            i, j = src[pos], src[pos + 1]; pos += 2
            off = i | ((j & 0xF0) << 4)
            for k in range((j & 0x0F) + 3):
                if len(out) >= outlen: break
                c = ring[(off + k) & 0xFFF]; out.append(c); ring[r] = c; r = (r + 1) & 0xFFF
    return bytes(out), pos
# updater:     lzss(flash, 0x4104,  0x00027D20)
# application: lzss(flash, 0x20104, 0x000C1EEC)
```

Results: [V]

| Payload | Size field | Output | Input consumed up to | Last data byte + 1 |
|---|---|---|---|---|
| updater | 0x27D20 | 163 104 bytes | 0x1E1C1 | 0x1E1C1 |
| application | 0xC1EEC | 794 348 bytes | 0x8E40F | 0x8E40F |

Both streams end exactly where the region's data ends. Both outputs start with the same
Blackfin startup code as the booter, have 0 invalid instructions in their first 16 KB,
and contain readable strings (`VS-100 Audio/MIDI`, `USB updater`, `run_vs100upd.dat`,
`USB Mass Storage`). Over the whole image, 7 % (updater) and 13.5 % (application) of
decoded "instructions" are invalid, consistent with trailing data sections. [I for the
last clause]
