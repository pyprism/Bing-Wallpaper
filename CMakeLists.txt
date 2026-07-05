cmake_minimum_required(VERSION 3.21)
project(bing-wallpaper LANGUAGES CXX)

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
endif()

qt_add_executable(bing-wallpaper
    MACOSX_BUNDLE
    WIN32
    ${SOURCES}
    ${HEADERS}
    resources.qrc
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
    )
endif()

enable_testing()
add_subdirectory(tests)
