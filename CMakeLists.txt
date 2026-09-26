cmake_minimum_required(VERSION 3.21)
project(bing-wallpaper LANGUAGES CXX VERSION 1.0.0)

if(WIN32)
    enable_language(RC)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(UNIX AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Source files here are UTF-8 and contain literal non-ASCII characters (©,
# …, —, ·) inside string literals (e.g. BingClient.cpp's copyright-mark
# split, "Open Gallery…"). Clang/GCC read source as UTF-8 by default; MSVC
# without a BOM reads it in the system code page (1252 on GitHub's Windows
# runners) unless told otherwise, silently mangling every one of those
# literals. This is harmless where Qt's own build already passes it.
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

set(SOURCES
    src/main.cpp
    src/BingClient.cpp
    src/WallpaperSetter.cpp
    src/TrayController.cpp
    src/Installer.cpp
    src/WallpaperLibrary.cpp
    src/GalleryWindow.cpp
)

set(HEADERS
    src/BingClient.h
    src/WallpaperSetter.h
    src/TrayController.h
    src/Installer.h
    src/WallpaperLibrary.h
    src/GalleryWindow.h
)

if(APPLE)
    set(MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in")

    # Generate the .icns app icon from the single source PNG at build time
    # rather than committing a generated binary asset to the repo.
    set(ICON_ICNS "${CMAKE_BINARY_DIR}/bing.icns")
    set(ICONSET_DIR "${CMAKE_BINARY_DIR}/bing.iconset")
    add_custom_command(
        OUTPUT "${ICON_ICNS}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ICONSET_DIR}"
        COMMAND sips -z 16 16     "${CMAKE_SOURCE_DIR}/icon/bing.png" --out "${ICONSET_DIR}/icon_16x16.png"
        COMMAND sips -z 32 32     "${CMAKE_SOURCE_DIR}/icon/bing.png" --out "${ICONSET_DIR}/icon_16x16@2x.png"
        COMMAND sips -z 32 32     "${CMAKE_SOURCE_DIR}/icon/bing.png" --out "${ICONSET_DIR}/icon_32x32.png"
        COMMAND sips -z 64 64     "${CMAKE_SOURCE_DIR}/icon/bing.png" --out "${ICONSET_DIR}/icon_32x32@2x.png"
        COMMAND sips -z 128 128   "${CMAKE_SOURCE_DIR}/icon/bing.png" --out "${ICONSET_DIR}/icon_128x128.png"
        COMMAND sips -z 256 256   "${CMAKE_SOURCE_DIR}/icon/bing.png" --out "${ICONSET_DIR}/icon_128x128@2x.png"
        COMMAND sips -z 256 256   "${CMAKE_SOURCE_DIR}/icon/bing.png" --out "${ICONSET_DIR}/icon_256x256.png"
        COMMAND sips -z 512 512   "${CMAKE_SOURCE_DIR}/icon/bing.png" --out "${ICONSET_DIR}/icon_256x256@2x.png"
        COMMAND sips -z 512 512   "${CMAKE_SOURCE_DIR}/icon/bing.png" --out "${ICONSET_DIR}/icon_512x512.png"
        COMMAND sips -z 1024 1024 "${CMAKE_SOURCE_DIR}/icon/bing.png" --out "${ICONSET_DIR}/icon_512x512@2x.png"
        COMMAND iconutil -c icns "${ICONSET_DIR}" -o "${ICON_ICNS}"
        DEPENDS "${CMAKE_SOURCE_DIR}/icon/bing.png"
        COMMENT "Generating bing.icns from icon/bing.png"
    )
    set_source_files_properties(${ICON_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

if(WIN32)
    configure_file(
        "${CMAKE_SOURCE_DIR}/packaging/windows/resource.rc.in"
        "${CMAKE_BINARY_DIR}/resource.rc"
        @ONLY
    )
    set(WIN_RESOURCE "${CMAKE_BINARY_DIR}/resource.rc")
endif()

qt_add_executable(bing-wallpaper
    MACOSX_BUNDLE
    WIN32
    ${SOURCES}
    ${HEADERS}
    resources.qrc
    ${ICON_ICNS}
    ${WIN_RESOURCE}
)

target_link_libraries(bing-wallpaper PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# Defaults to the project() version above for a plain dev build; the release
# workflow overrides it with the actual release tag:
#   cmake -S . -B build -DAPP_VERSION_STRING=1.2.3 ...
# Release tags in this repo look like "v1.2.3" (see packaging/*/package.sh
# and installer.iss, which keep the "v" in filenames/AppVersion on purpose);
# strip a leading v/V here so this one value can *also* feed
# MACOSX_BUNDLE_SHORT_VERSION_STRING/MACOSX_BUNDLE_BUNDLE_VERSION below
# (which Apple expects as a plain dotted number) and the About dialog (which
# prepends its own "v" — "v1.2.3", not "vv1.2.3").
if(NOT DEFINED APP_VERSION_STRING)
    set(APP_VERSION_STRING "${PROJECT_VERSION}")
endif()
string(REGEX REPLACE "^[vV]" "" APP_VERSION_STRING "${APP_VERSION_STRING}")
target_compile_definitions(bing-wallpaper PRIVATE APP_VERSION="${APP_VERSION_STRING}")

if(WIN32)
    target_link_libraries(bing-wallpaper PRIVATE user32)
endif()

if(APPLE)
    set_target_properties(bing-wallpaper PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Bing Wallpaper"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.bing-wallpaper"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in"
        MACOSX_BUNDLE_ICON_FILE "bing.icns"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${APP_VERSION_STRING}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${APP_VERSION_STRING}"
    )
endif()

enable_testing()
add_subdirectory(tests)
