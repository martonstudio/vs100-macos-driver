# F17: loopback alignment jitter

Verdict: not a driver or device fault. It was a measurement artifact in tools/rtl_test.c.

- The old tool located each click by its maximum sample. The VS-100 loopback path filters
  the click, so the maximum wanders by up to ~20 samples (peak offsets +22..+42 seen).
- The onset (first sample above 25% of peak) was +21/+22 in every click of the same run.
- tools/rtl_test.c now measures the interpolated threshold crossing. `./build/rtl_test N 9`
  at buffers 32/64/128/512: jitter (max-min) over 8 clicks = 0.00 samples every run.
  Latency to the loopback input 9 is ~0.42-0.44 ms (about 20-21 samples).
- Engine stats are not readable during rtl_test (coreaudiod owns the engine); not needed
  because the onset is sample-exact.
