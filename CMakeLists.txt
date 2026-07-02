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
    
    # Strip removed legacy AGL framework from Qt6 transitive dependencies to prevent linker errors on macOS 14+
    if(TARGET Qt6::Gui)
        get_target_property(qt6_gui_libs Qt6::Gui INTERFACE_LINK_LIBRARIES)
        if(qt6_gui_libs)
            list(REMOVE_ITEM qt6_gui_libs "-framework AGL")
            list(REMOVE_ITEM qt6_gui_libs "AGL::AGL")
            set_target_properties(Qt6::Gui PROPERTIES INTERFACE_LINK_LIBRARIES "${qt6_gui_libs}")
        endif()
    endif()
endif()

target_link_libraries(YouTubeTranscribeExtractor PRIVATE Qt6::Widgets)
