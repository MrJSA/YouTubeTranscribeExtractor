cmake_minimum_required(VERSION 4.2)
project(YouTubeTranscribeExtractor)

set(CMAKE_CXX_STANDARD 20)

add_executable(YouTubeTranscribeExtractor WIN32 main.cpp)
target_link_libraries(YouTubeTranscribeExtractor PRIVATE comctl32 comdlg32 dwmapi)
