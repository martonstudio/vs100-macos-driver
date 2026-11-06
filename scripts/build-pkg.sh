#!/bin/bash
# Builds build/VS100-Driver-<version>.pkg from the already-built build/ outputs (make pkg).
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${1:-0.1.0}"
B=build
# staged outside Dropbox (its xattrs would be packed as ._ files); /tmp is cleaned by macOS
STAGE=$(mktemp -d /tmp/vs100pkg.XXXXXX); ROOT=$STAGE/pkgroot; SUP="$ROOT/Library/Application Support/VS100"
export COPYFILE_DISABLE=1

for f in $B/VS100.driver $B/vs100_midid "$B/VS100 Control.app"; do [ -e "$f" ] || { echo "missing $f: run make first"; exit 1; }; done

# Stage the payload (fresh temp dir each run)
mkdir -p "$ROOT/Library/Audio/Plug-Ins/HAL" "$ROOT/Library/LaunchAgents" "$SUP" "$ROOT/Applications"
cp -R $B/VS100.driver "$ROOT/Library/Audio/Plug-Ins/HAL/"
cp $B/vs100_midid "$SUP/"
cp scripts/uninstall.sh "$SUP/"
cp midi/com.vs100.midid.plist "$ROOT/Library/LaunchAgents/"
cp -R "$B/VS100 Control.app" "$ROOT/Applications/"
cp "scripts/Uninstall VS-100 Driver.command" "$ROOT/Applications/"
chmod 755 "$SUP/uninstall.sh" "$SUP/vs100_midid" "$ROOT/Applications/Uninstall VS-100 Driver.command"
chmod 755 scripts/pkg-scripts/postinstall

# Ad-hoc signatures only (no Developer ID available)
codesign --force --sign - "$SUP/vs100_midid"
codesign --force --sign - "$ROOT/Library/Audio/Plug-Ins/HAL/VS100.driver"
codesign --force --sign - "$ROOT/Applications/VS100 Control.app"

# Strip xattrs (Dropbox/Finder metadata would be packed as ._ files) and make bundles non-relocatable
xattr -cr "$ROOT"
pkgbuild --analyze --root "$ROOT" "$B/components.plist" >/dev/null
N=$(/usr/libexec/PlistBuddy -c "Print" "$B/components.plist" | grep -c RootRelativeBundlePath)
for i in $(seq 0 $((N - 1))); do
    for kv in BundleIsRelocatable BundleIsVersionChecked; do
        /usr/libexec/PlistBuddy -c "Set :$i:$kv false" "$B/components.plist" 2>/dev/null || /usr/libexec/PlistBuddy -c "Add :$i:$kv bool false" "$B/components.plist"
    done
done
pkgbuild --root "$ROOT" --component-plist "$B/components.plist" --identifier com.vs100.driver.pkg --version "$VERSION" \
    --ownership recommended --scripts scripts/pkg-scripts --install-location / "$B/VS100-component.pkg"
productbuild --package "$B/VS100-component.pkg" "$B/VS100-Driver-$VERSION.pkg"
echo "built $B/VS100-Driver-$VERSION.pkg"
