#!/bin/bash
# Gather the built plugins into:
#   dist/VocalTune.pkg            the installer (main download)
#   dist/VocalTune for Mac.zip    the same plugins for installing by hand, plus the standalone app
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=$(sed -nE 's/^project\(VOCALTUNE VERSION ([0-9.]+).*/\1/p' CMakeLists.txt)
ART="build/VocalTune_artefacts/Release"
OUT="dist/VocalTune for Mac"
PKGW="build/pkg"
rm -rf dist "$PKGW"
mkdir -p "$OUT" "$PKGW/vst3" "$PKGW/au" "$PKGW/resources"

ditto "$ART/VST3/VocalTune.vst3" "$OUT/VocalTune.vst3"
ditto "$ART/AU/VocalTune.component" "$OUT/VocalTune.component"
ditto "$ART/Standalone/VocalTune.app" "$OUT/Extras/VocalTune.app"

# Signing: with an Apple "Developer ID Application" identity in VT_SIGN_APP the code is signed for
# notarization (hardened runtime + secure timestamp); without one it is signed "ad hoc", which
# Apple Silicon Macs need to load it at all.
for b in "$OUT/VocalTune.vst3" "$OUT/VocalTune.component" "$OUT/Extras/VocalTune.app"; do
  if [ -n "${VT_SIGN_APP:-}" ]; then
    codesign --force --deep --options runtime --timestamp --sign "$VT_SIGN_APP" "$b"
  else
    codesign --force --deep --sign - "$b"
  fi
  codesign --verify --deep --strict "$b"
done
echo "Architectures:"
lipo -info "$OUT/VocalTune.vst3/Contents/MacOS/VocalTune"
lipo -info "$OUT/VocalTune.component/Contents/MacOS/VocalTune"

# ---------------------------------------------------------------- the .pkg installer
ditto "$OUT/VocalTune.vst3" "$PKGW/vst3/VocalTune.vst3"
ditto "$OUT/VocalTune.component" "$PKGW/au/VocalTune.component"
for k in vst3 au; do
  pkgbuild --analyze --root "$PKGW/$k" "$PKGW/$k.plist"
  # always install into /Library/Audio/Plug-Ins, even if another copy of the bundle exists somewhere else
  n=$(/usr/libexec/PlistBuddy -c "Print" "$PKGW/$k.plist" | grep -c "BundleIsRelocatable" || true)
  for ((i = 0; i < n; i++)); do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$PKGW/$k.plist"
  done
done
pkgbuild --root "$PKGW/vst3" --component-plist "$PKGW/vst3.plist" --identifier com.bassbible.vocaltune.vst3 \
  --version "$VERSION" --install-location "/Library/Audio/Plug-Ins/VST3" --scripts packaging/pkg/scripts "$PKGW/vst3.pkg"
pkgbuild --root "$PKGW/au" --component-plist "$PKGW/au.plist" --identifier com.bassbible.vocaltune.au \
  --version "$VERSION" --install-location "/Library/Audio/Plug-Ins/Components" --scripts packaging/pkg/scripts "$PKGW/au.pkg"
sed "s/__VERSION__/$VERSION/g" packaging/pkg/distribution.xml > "$PKGW/distribution.xml"
for f in packaging/pkg/resources/*.html; do sed "s/__VERSION__/$VERSION/g" "$f" > "$PKGW/resources/$(basename "$f")"; done
cp brand/icon-256.png "$PKGW/resources/icon.png"
# with a "Developer ID Installer" identity in VT_SIGN_INSTALLER the installer itself is signed too
SIGN_PKG=()
[ -n "${VT_SIGN_INSTALLER:-}" ] && SIGN_PKG=(--sign "$VT_SIGN_INSTALLER" --timestamp)
productbuild --distribution "$PKGW/distribution.xml" --resources "$PKGW/resources" --package-path "$PKGW" \
  ${SIGN_PKG[@]+"${SIGN_PKG[@]}"} "dist/VocalTune.pkg"
pkgutil --payload-files "dist/VocalTune.pkg" | grep -E "\.(vst3|component)$" || true

# ---------------------------------------------------------------- the zip (install by hand)
cp packaging/README.txt "$OUT/README.txt"
(cd dist && ditto -c -k --sequesterRsrc --keepParent "VocalTune for Mac" "VocalTune for Mac.zip")
ls -la dist
