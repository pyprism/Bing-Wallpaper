find_package(Qt6 REQUIRED COMPONENTS Test Network Gui)

qt_add_executable(tst_BingClient
    tst_BingClient.cpp
    ${CMAKE_SOURCE_DIR}/src/BingClient.cpp
    ${CMAKE_SOURCE_DIR}/src/WallpaperLibrary.cpp
)
target_include_directories(tst_BingClient PRIVATE ${CMAKE_SOURCE_DIR}/src)
# Gui is needed because BingClient validates downloaded bytes with
# QImageReader before caching them (a truncated/HTML response must not be
# accepted as a wallpaper image).
target_link_libraries(tst_BingClient PRIVATE Qt6::Test Qt6::Network Qt6::Core Qt6::Gui)
add_test(NAME BingClientTest COMMAND tst_BingClient)
set_tests_properties(BingClientTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_WallpaperSetter
    tst_WallpaperSetter.cpp
    ${CMAKE_SOURCE_DIR}/src/WallpaperSetter.cpp
)
target_include_directories(tst_WallpaperSetter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_WallpaperSetter PRIVATE Qt6::Test Qt6::Gui Qt6::Core)
if(WIN32)
    target_link_libraries(tst_WallpaperSetter PRIVATE user32)
endif()
add_test(NAME WallpaperSetterTest COMMAND tst_WallpaperSetter)
set_tests_properties(WallpaperSetterTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_WallpaperLibrary
    tst_WallpaperLibrary.cpp
    ${CMAKE_SOURCE_DIR}/src/WallpaperLibrary.cpp
)
target_include_directories(tst_WallpaperLibrary PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_WallpaperLibrary PRIVATE Qt6::Test Qt6::Core)
add_test(NAME WallpaperLibraryTest COMMAND tst_WallpaperLibrary)
set_tests_properties(WallpaperLibraryTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(tst_Installer
    tst_Installer.cpp
    ${CMAKE_SOURCE_DIR}/src/Installer.cpp
)
target_include_directories(tst_Installer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_Installer PRIVATE Qt6::Test Qt6::Core)
add_test(NAME InstallerTest COMMAND tst_Installer)
set_tests_properties(InstallerTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
