# NEXT

Step 2 of PLAN.md: MIDI bridge. Adapt midi/ua4fx_midid.c to 0582:00EB, bulk OUT 0x04 /
IN 0x83, publish one CoreMIDI endpoint pair per cable (0 = MIDI port, 1 = control surface),
install as a launchd user agent.

Done = MIDI Monitor shows fader/transport events from "VS-100 Control" and a DAW's Mackie
Control setup sees the transport buttons.
