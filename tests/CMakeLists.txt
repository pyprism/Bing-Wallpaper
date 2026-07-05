find_package(Qt6 REQUIRED COMPONENTS Test Network Gui)

qt_add_executable(tst_BingClient
    tst_BingClient.cpp
    ${CMAKE_SOURCE_DIR}/src/BingClient.cpp
)
target_include_directories(tst_BingClient PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tst_BingClient PRIVATE Qt6::Test Qt6::Network Qt6::Core)
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
