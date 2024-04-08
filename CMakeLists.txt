cmake_minimum_required(VERSION 3.10)
project(study.ffmpeg)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(Boost_USE_STATIC_LIBS ON)
set(BOOST_ROOT $ENV{BOOST_LATEST})
include_directories(${BOOST_ROOT})

find_package(FFMPEG REQUIRED)
include_directories(${FFMPEG_INCLUDE_DIRS})
link_directories(${FFMPEG_LIBRARY_DIRS})

find_package(fmt CONFIG REQUIRED)
add_compile_definitions(DFMT_HEADER_ONLY=1)

find_package(spdlog CONFIG REQUIRED)
include_directories($ENV{SIMPLE_CPP})

add_executable(test_encode test_encode.cpp)
target_link_libraries(test_encode PRIVATE ${FFMPEG_LIBRARIES})

add_executable(test_decode test_decode.cpp)
target_link_libraries(test_decode PRIVATE ${FFMPEG_LIBRARIES})

add_subdirectory(qtexamples)


add_executable(rc2rb rowcross2rowblock.cpp)

add_executable(video_in_pcm video_in_pcm.cpp)
target_link_libraries(video_in_pcm PRIVATE ${FFMPEG_LIBRARIES} fmt::fmt-header-only)



### vcpkg install ffmpeg[fdk-aac,fontconfig,freetype,fribidi,gpl,mp3lame,nvcodec,vorbis,vpx,webp,x264,x265] 


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

add_executable(test_sti_read test_sti_read.cpp)
target_link_libraries(test_sti_read PRIVATE fmt::fmt-header-only)


add_subdirectory(ffexamples)
add_subdirectory(demo)

