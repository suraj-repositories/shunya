#!/usr/bin/env bash
# ==============================================================================
# Automated standalone packaging script for Shunya Engineering Studio on Linux.
# Generates portable AppImage directory structure and standalone tarball release.
# Fulfills Phase 4 Roadmap Task PH4-13.04.005.
# ==============================================================================

set -euo pipefail

VERSION="${1:-4.0.0}"
BUILD_DIR="${2:-build}"
DIST_DIR="dist/shunya-linux-x86_64"
APPDIR="dist/AppDir"

echo "===================================================="
echo " Packaging Shunya Engineering Studio v${VERSION} (Linux x86_64)"
echo "===================================================="

rm -rf "${DIST_DIR}" "${APPDIR}" "dist/shunya-v${VERSION}-linux-x86_64.tar.gz"
mkdir -p "${DIST_DIR}/bin" "${DIST_DIR}/data"
mkdir -p "${APPDIR}/usr/bin" "${APPDIR}/usr/share/shunya/data"

echo "[1/4] Copying binary executables and shared libraries..."
cp -f "${BUILD_DIR}/apps/studio/appshunya" "${DIST_DIR}/bin/" || true
cp -f "${BUILD_DIR}/apps/cli/shunya-cli" "${DIST_DIR}/bin/" || true
cp -f "${BUILD_DIR}/core/base/libshunya_core_base.so" "${DIST_DIR}/bin/" || true
cp -f "${BUILD_DIR}/core/domain/libshunya_core_domain.so" "${DIST_DIR}/bin/" || true

echo "[2/4] Packaging data assets and brand icons..."
if [ -f "data/elements.csv" ]; then
    cp -f "data/elements.csv" "${DIST_DIR}/data/"
    cp -f "data/elements.csv" "${APPDIR}/usr/share/shunya/data/"
fi
mkdir -p "${APPDIR}/usr/share/icons/hicolor/scalable/apps"
if [ -f "assets/icons/sunken-eye.svg" ]; then
    cp -f "assets/icons/sunken-eye.svg" "${APPDIR}/usr/share/icons/hicolor/scalable/apps/shunya.svg"
fi
for sz in 16 24 32 48 64 128 256 512; do
    if [ -f "assets/icons/shunya_${sz}x${sz}.png" ]; then
        mkdir -p "${APPDIR}/usr/share/icons/hicolor/${sz}x${sz}/apps"
        cp -f "assets/icons/shunya_${sz}x${sz}.png" "${APPDIR}/usr/share/icons/hicolor/${sz}x${sz}/apps/shunya.png"
    fi
done

echo "[3/4] Creating AppRun desktop entry..."
cat << 'EOF' > "${APPDIR}/AppRun"
#!/bin/sh
SELF=$(readlink -f "$0")
HERE=${SELF%/*}
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH:-}"
exec "${HERE}/usr/bin/appshunya" "$@"
EOF
chmod +x "${APPDIR}/AppRun"

cat << EOF > "${APPDIR}/shunya.desktop"
[Desktop Entry]
Type=Application
Name=Shunya Engineering Studio
Exec=appshunya
Icon=shunya
Categories=Development;Engineering;Science;
EOF

echo "[4/4] Creating standalone tarball release..."
tar -czf "dist/shunya-v${VERSION}-linux-x86_64.tar.gz" -C dist shunya-linux-x86_64

echo "===================================================="
echo " Linux standalone packaging complete!"
echo "===================================================="
