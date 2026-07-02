cmake_minimum_required(VERSION 3.16)
project(YouTubeTranscribeExtractor)

set(CMAKE_CXX_STANDARD 20)

# Qt6 automatic build tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

if(WIN32)
    set(HELPER_SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dist/get_transcript.exe")
else()
    set(HELPER_SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dist/get_transcript")
endif()

configure_file(resources.qrc.in ${CMAKE_CURRENT_BINARY_DIR}/resources.qrc)

add_executable(YouTubeTranscribeExtractor main.cpp ${CMAKE_CURRENT_BINARY_DIR}/resources.qrc)

# Configure macOS Bundle packaging
if(APPLE)
    set_target_properties(YouTubeTranscribeExtractor PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "YouTubeTranscribeExtractor"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.joshua.YouTubeTranscribeExtractor"
    )
endif()

target_link_libraries(YouTubeTranscribeExtractor PRIVATE Qt6::Widgets)
