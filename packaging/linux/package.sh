#!/usr/bin/env bash
# Packages the built Linux binary into an AppImage.
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
# linuxdeploy and its plugins are AppImages themselves; run without needing FUSE (CI containers)
export APPIMAGE_EXTRACT_AND_RUN=1
NO_STRIP=1 linuxdeploy \
    --appdir "${APPDIR}" \
    --plugin qt \
    --desktop-file "packaging/linux/bing-wallpaper.desktop" \
    --icon-file "icon/bing.png" \
    --output appimage

# linuxdeploy names the output after the .desktop `Name=` ("Bing_Wallpaper-<ver>-<arch>.AppImage"),
# not the binary, so match case-insensitively and fail loudly instead of silently shipping no AppImage.
APPIMAGE_OUT="$(find . -maxdepth 1 -iname 'bing*wallpaper*.AppImage' ! -name "${APP_NAME}-${VERSION}-linux-${ARCH}.AppImage" | head -n1)"
if [ -n "${APPIMAGE_OUT}" ]; then
    mv "${APPIMAGE_OUT}" "${APP_NAME}-${VERSION}-linux-${ARCH}.AppImage"
fi
if [ ! -f "${APP_NAME}-${VERSION}-linux-${ARCH}.AppImage" ]; then
    echo "error: linuxdeploy produced no AppImage" >&2
    exit 1
fi
chmod +x "${APP_NAME}-${VERSION}-linux-${ARCH}.AppImage"

echo "Done."
