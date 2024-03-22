cmake_minimum_required(VERSION 3.10)
project(study.ffmpeg)

set(CMAKE_CXX_STANDARD 20)


find_package(FFMPEG REQUIRED)
include_directories(${FFMPEG_INCLUDE_DIRS})
link_directories(${FFMPEG_LIBRARY_DIRS})

include_directories($ENV{BOOST_LATEST})
link_directories($ENV{BOOST_LATEST}/lib64-msvc-14.2)
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


add_executable(ffplay fftools/ffplay.c fftools/ffplay_renderer.c fftools/opt_common.c fftools/cmdutils.c)
target_include_directories(ffplay PRIVATE fftools)
find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(ffplay 
    PRIVATE
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    ${FFMPEG_LIBRARIES}
)

add_executable(ffprobe fftools/ffprobe.c fftools/opt_common.c fftools/cmdutils.c)
target_include_directories(ffprobe PRIVATE fftools)
find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(ffprobe 
    PRIVATE
    ${FFMPEG_LIBRARIES}
)

 
#add_subdirectory(ffplayer-main)
