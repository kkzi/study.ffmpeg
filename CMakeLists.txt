cmake_minimum_required(VERSION 3.10)
project(study.ffmpeg)

set(CMAKE_CXX_STANDARD 20)


find_package(FFMPEG REQUIRED)
include_directories(${FFMPEG_INCLUDE_DIRS})
link_directories(${FFMPEG_LIBRARY_DIRS})

include_directories($ENV{boost})
find_package(fmt CONFIG REQUIRED)
add_compile_definitions(DFMT_HEADER_ONLY)

add_executable(test_encode test_encode.cpp)
target_link_libraries(test_encode PRIVATE ${FFMPEG_LIBRARIES})

add_executable(test_decode test_decode.cpp)
target_link_libraries(test_decode PRIVATE ${FFMPEG_LIBRARIES})

add_subdirectory(qtexamples)


add_executable(rc2rb rowcross2rowblock.cpp)

add_executable(video_in_pcm video_in_pcm.cpp)
target_link_libraries(video_in_pcm PRIVATE ${FFMPEG_LIBRARIES} fmt::fmt-header-only)


