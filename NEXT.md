# NEXT

Finish-line check (PLAN.md): with headphones on the VS-100 phones out, record and play back in a
DAW for 10+ minutes at 128 samples, and map "VS-100 Control" as a Mackie Control surface.

Done = clean audio on headphones and inputs for 10 min (no clicks/dropouts), transport buttons
drive the DAW, and `/usr/bin/log show --last 15m --predicate 'subsystem == "com.vs100.driver"'`
shows no resyncs or errors.
