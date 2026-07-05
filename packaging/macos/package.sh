#!/usr/bin/env bash
# Packages the built .app bundle into an unsigned .dmg.
# Usage: packaging/macos/package.sh [build-dir] [version]
set -euo pipefail

BUILD_DIR="${1:-build}"
VERSION="${2:-dev}"
APP_NAME="bing-wallpaper"
APP_BUNDLE="${BUILD_DIR}/${APP_NAME}.app"
DMG_NAME="${APP_NAME}-${VERSION}-macos.dmg"

if [ ! -d "${APP_BUNDLE}" ]; then
    echo "error: ${APP_BUNDLE} not found — build first (cmake --build ${BUILD_DIR})" >&2
    exit 1
fi

echo "Stripping symbols..."
strip -x "${APP_BUNDLE}/Contents/MacOS/${APP_NAME}"

echo "Running macdeployqt..."
macdeployqt "${APP_BUNDLE}"

echo "Creating ${DMG_NAME}..."
rm -f "${DMG_NAME}"
hdiutil create -volname "Bing Wallpaper" -srcfolder "${APP_BUNDLE}" -ov -format UDZO "${DMG_NAME}"

echo "Done: ${DMG_NAME}"
echo "Unsigned build — first launch requires right-click > Open to bypass Gatekeeper."
