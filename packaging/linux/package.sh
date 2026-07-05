#!/usr/bin/env bash
# Packages the built Linux binary into an AppImage (and a plain .tar.gz).
# Requires `linuxdeploy` and `linuxdeploy-plugin-qt` on PATH (or in the CWD).
# Usage: packaging/linux/package.sh [build-dir] [version]
set -euo pipefail

BUILD_DIR="${1:-build}"
VERSION="${2:-dev}"
APP_NAME="bing-wallpaper"
BINARY="${BUILD_DIR}/${APP_NAME}"
APPDIR="${BUILD_DIR}/AppDir"

case "$(uname -m)" in
    x86_64) ARCH="x86_64" ;;
    aarch64|arm64) ARCH="arm64" ;;
    *) ARCH="$(uname -m)" ;;
esac

if [ ! -x "${BINARY}" ]; then
    echo "error: ${BINARY} not found — build first (cmake --build ${BUILD_DIR})" >&2
    exit 1
fi

rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin" "${APPDIR}/usr/share/applications" "${APPDIR}/usr/share/icons/hicolor/256x256/apps"

cp "${BINARY}" "${APPDIR}/usr/bin/${APP_NAME}"
cp packaging/linux/bing-wallpaper.desktop "${APPDIR}/usr/share/applications/"
cp icon/bing.png "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${APP_NAME}.png"

echo "Running linuxdeploy..."
export QML_SOURCES_PATHS=""
export VERSION
NO_STRIP=1 linuxdeploy \
    --appdir "${APPDIR}" \
    --plugin qt \
    --desktop-file "packaging/linux/bing-wallpaper.desktop" \
    --icon-file "icon/bing.png" \
    --output appimage

mv "${APP_NAME}"*.AppImage "${APP_NAME}-${VERSION}-linux-${ARCH}.AppImage" 2>/dev/null || true

echo "Creating tar.gz fallback..."
tar -C "${BUILD_DIR}" -czf "${APP_NAME}-${VERSION}-linux-${ARCH}.tar.gz" "${APP_NAME}"

echo "Done."
