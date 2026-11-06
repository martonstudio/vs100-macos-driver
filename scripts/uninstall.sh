#!/bin/bash
# Removes the VS-100 HAL plug-in (sudo) and the MIDI bridge launch agent.
set -uo pipefail
HAL_DIR=/Library/Audio/Plug-Ins/HAL
AGENT="$HOME/Library/LaunchAgents/com.vs100.midid.plist"
APP_SUPPORT="$HOME/Library/Application Support/VS100"

launchctl bootout "gui/$(id -u)/com.vs100.midid" 2>/dev/null || true
rm -f "$AGENT"
rm -rf "$APP_SUPPORT" "$HOME/Applications/VS100 Control.app"

if [ -d "$HAL_DIR/VS100.driver" ]; then
    echo "==> Removing $HAL_DIR/VS100.driver (sudo)"
    sudo rm -rf "$HAL_DIR/VS100.driver"
    sudo launchctl kickstart -k system/com.apple.audio.coreaudiod 2>/dev/null || sudo killall coreaudiod
fi
echo "done"
