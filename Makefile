# VS-100 user-space driver for macOS (Apple silicon / Intel)
#
#   make            build everything into ./build
#   make driver     CoreAudio HAL plug-in  -> build/VS100.driver
#   make midi       CoreMIDI bridge daemon -> build/vs100_midid
#   make tools      engine test + HAL harness
#   make pkg        build/VS100-Driver-<version>.pkg (HAL plug-in, MIDI bridge + agent, control app)
#   make install    (sudo) build and install the pkg
#   make uninstall  (sudo) remove everything the pkg installed
#
CC       ?= clang
ARCHS    ?= $(shell uname -m)
MINOS    ?= 13.0
CFLAGS   ?= -O2
CFLAGS   += -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations -fblocks \
            -mmacosx-version-min=$(MINOS) $(addprefix -arch ,$(ARCHS))
FW_BASE   = -framework CoreFoundation -framework IOKit
BUILD     = build
BUNDLE    = $(BUILD)/VS100.driver
ENGINE    = driver/VS100_USB.c
HDRS      = driver/VS100_USB.h driver/VS100_Log.h

all: driver midi tools ui

driver: $(BUNDLE)/Contents/MacOS/VS100

$(BUNDLE)/Contents/MacOS/VS100: driver/VS100_Driver.c $(ENGINE) $(HDRS) driver/Info.plist
	@mkdir -p $(BUNDLE)/Contents/MacOS $(BUNDLE)/Contents/Resources
	cp driver/Info.plist $(BUNDLE)/Contents/Info.plist
	$(CC) $(CFLAGS) -bundle -o $@ driver/VS100_Driver.c $(ENGINE) $(FW_BASE) -framework CoreAudio
	codesign --force --sign - $(BUNDLE)

midi: $(BUILD)/vs100_midid

$(BUILD)/vs100_midid: midi/vs100_midid.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DVS100_LOG_STDERR=1 -o $@ midi/vs100_midid.c $(FW_BASE) -framework CoreMIDI
	codesign --force --sign - $@

tools: $(BUILD)/vs100_test $(BUILD)/hal_harness $(BUILD)/rtl_test $(BUILD)/vs100_midi_listen

$(BUILD)/vs100_midi_listen: tools/vs100_midi_listen.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ tools/vs100_midi_listen.c -framework CoreFoundation -framework CoreMIDI

$(BUILD)/rtl_test: tools/rtl_test.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ tools/rtl_test.c $(FW_BASE) -framework CoreAudio

$(BUILD)/vs100_test: tools/vs100_test.c $(ENGINE) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DVS100_LOG_STDERR=1 -o $@ tools/vs100_test.c $(ENGINE) $(FW_BASE)

$(BUILD)/hal_harness: tools/hal_harness.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ tools/hal_harness.c $(FW_BASE) -framework CoreAudio

UIAPP = $(BUILD)/VS100\ Control.app
ui: $(BUILD)/VS100\ Control.app/Contents/MacOS/VS100Control

$(BUILD)/VS100\ Control.app/Contents/MacOS/VS100Control: ui/VS100Control.swift ui/Info.plist
	@mkdir -p "$(BUILD)/VS100 Control.app/Contents/MacOS" "$(BUILD)/VS100 Control.app/Contents/Resources"
	cp ui/Info.plist "$(BUILD)/VS100 Control.app/Contents/Info.plist"
	swiftc -O -parse-as-library -target $(firstword $(ARCHS))-apple-macos$(MINOS) -o "$@" ui/VS100Control.swift -framework SwiftUI -framework CoreAudio -framework CoreMIDI -framework AppKit
	codesign --force --sign - "$(BUILD)/VS100 Control.app"

VERSION  ?= 0.1.0
pkg: driver midi ui
	chmod +x scripts/build-pkg.sh && scripts/build-pkg.sh $(VERSION)

install: pkg
	sudo installer -pkg $(BUILD)/VS100-Driver-$(VERSION).pkg -target /

uninstall:
	sudo "/Library/Application Support/VS100/uninstall.sh"

clean:
	rm -rf $(BUILD)

.PHONY: all driver midi tools ui pkg install uninstall clean
