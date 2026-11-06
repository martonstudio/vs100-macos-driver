#!/bin/bash
# Double-click to uninstall the VS-100 driver (asks for your password in this Terminal window).
echo "This removes the VS-100 audio driver, MIDI bridge and control app."
read -r -p "Continue? [y/N] " a
[ "$a" = "y" ] || [ "$a" = "Y" ] || { echo "Cancelled."; exit 0; }
sudo "/Library/Application Support/VS100/uninstall.sh"
echo; read -r -n 1 -p "Press any key to close."
