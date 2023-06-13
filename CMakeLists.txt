cmake_minimum_required(VERSION 3.10)
project(study.ffmpeg)

set(CMAKE_CXX_STANDARD 20)


find_package(FFMPEG REQUIRED)
include_directories(${FFMPEG_INCLUDE_DIRS})
link_directories(${FFMPEG_LIBRARY_DIRS})

add_executable(test1 test1.cpp)
target_link_libraries(test1 PRIVATE ${FFMPEG_LIBRARIES})

add_executable(test_encode test_encode.cpp)
target_link_libraries(test_encode PRIVATE ${FFMPEG_LIBRARIES})

add_executable(test_decode test_decode.cpp)
target_link_libraries(test_decode PRIVATE ${FFMPEG_LIBRARIES})
