cmake_minimum_required(VERSION 3.21)
project(bing-wallpaper LANGUAGES CXX)

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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

set(SOURCES
    src/main.cpp
    src/BingClient.cpp
    src/WallpaperSetter.cpp
    src/TrayController.cpp
    src/Installer.cpp
)

set(HEADERS
    src/BingClient.h
    src/WallpaperSetter.h
    src/TrayController.h
    src/Installer.h
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

if(WIN32)
    target_link_libraries(bing-wallpaper PRIVATE user32)
endif()

if(APPLE)
    set_target_properties(bing-wallpaper PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Bing Wallpaper"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.bing-wallpaper"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in"
        MACOSX_BUNDLE_ICON_FILE "bing.icns"
    )
endif()

enable_testing()
add_subdirectory(tests)
