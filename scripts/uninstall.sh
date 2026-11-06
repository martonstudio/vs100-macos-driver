#!/bin/bash
# Uninstalls the VS-100 driver: HAL plug-in, MIDI bridge + LaunchAgent, control app, pkg receipt.
# Also removes leftovers of the old UA4FX-named build. Needs root; re-runs itself with sudo.
#   sudo "/Library/Application Support/VS100/uninstall.sh"
if [ "$(id -u)" -ne 0 ]; then exec sudo "$0" "$@"; fi

CUSER=$(stat -f %Su /dev/console 2>/dev/null); [ "$CUSER" = "root" ] && CUSER=""
CUID=""; CHOME=""
if [ -n "$CUSER" ]; then CUID=$(id -u "$CUSER"); CHOME=$(dscl . -read "/Users/$CUSER" NFSHomeDirectory | awk '{print $2}'); fi

echo "==> Unloading MIDI bridge"
[ -n "$CUID" ] && for l in com.vs100.midid com.ua4fx.midid; do launchctl bootout "gui/$CUID/$l" 2>/dev/null; done
pkill -x vs100_midid 2>/dev/null

echo "==> Removing files"
rm -f /Library/LaunchAgents/com.vs100.midid.plist /Library/LaunchAgents/com.ua4fx.*.plist
rm -rf "/Library/Audio/Plug-Ins/HAL/VS100.driver" "/Library/Audio/Plug-Ins/HAL/UA4FX.driver"
rm -rf "/Applications/VS100 Control.app" "/Applications/Uninstall VS-100 Driver.command"
if [ -n "$CHOME" ]; then
    rm -f "$CHOME/Library/LaunchAgents/com.ua4fx.midid.plist"
    rm -rf "$CHOME/Library/Application Support/UA4FX" "$CHOME/Applications/UA4FX Control.app"
fi
rm -f /tmp/vs100_midid.log

echo "==> Forgetting pkg receipt"
pkgutil --forget com.vs100.driver.pkg >/dev/null 2>&1

echo "==> Restarting coreaudiod"
killall coreaudiod 2>/dev/null

# Remove the support dir (and this script) last.
rm -rf "/Library/Application Support/VS100"
echo "VS-100 driver removed."
exit 0
