cmake_minimum_required(VERSION 3.21)
project(UsbCam VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets)

qt_add_executable(UsbCam
    MACOSX_BUNDLE
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
)

target_link_libraries(UsbCam PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets)

set_target_properties(UsbCam PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Info.plist.in
    WIN32_EXECUTABLE ON
)

# macOS: re-sign the bundle so Info.plist is bound to the signature; otherwise TCC
# never shows the camera permission prompt and the preview stays blank.
if(APPLE)
    add_custom_command(TARGET UsbCam POST_BUILD
        COMMAND codesign --force --deep --sign - $<TARGET_BUNDLE_DIR:UsbCam>
        COMMENT "Ad-hoc signing UsbCam.app")
endif()
