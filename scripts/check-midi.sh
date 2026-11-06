#!/bin/bash
# Prints MIDI from the VS-100 for 15 s. Move a fader and press transport buttons while it runs.
cd "$(dirname "$0")/.." && make -s tools >/dev/null && exec ./build/vs100_midi_listen "${1:-15}"
