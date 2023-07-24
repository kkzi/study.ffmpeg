cmake_minimum_required(VERSION 3.10)
project(ffmpeg-study)

set(CMAKE_CXX_STANDARD 23)

add_link_options(/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)

set(QT_DIR $ENV{QTDIR})
set(CMAKE_PREFIX_PATH ${QT_DIR}/lib/cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
find_package(Qt5 CONFIG COMPONENTS Core Widgets Network)

include_directories($ENV{BOOST})
link_directories($ENV{BOOST}/lib64-msvc-14.2)


#set(FFMPEG_DIR D:/Downloads/ffmpeg-5.1.2)
#include_directories(
#    ${FFMPEG_DIR} 
#    ${FFMPEG_DIR}/ffbuild/release/include
#    ${FFMPEG_DIR}/compat/atomics/win32
#)
##link_directories(${FFMPEG_DIR}/ffbuild/debug/bin)
#link_directories(${FFMPEG_DIR}/ffbuild/release/lib)

find_package(FFMPEG REQUIRED)

include_directories(${FFMPEG_INCLUDE_DIRS})
link_directories   (${FFMPEG_LIBRARY_DIRS})


#add_executable(ffplayer main.cpp ffplayer.cpp)
#add_executable(TestVideoRecv- VideoRecvTest.cpp FfplayWidget.cpp VideoDecoder.cpp)
##target_link_libraries(TestVideoRecv- avcodec avformat avutil swscale Qt::Widgets)
#target_include_directories(TestVideoRecv- PRIVATE ${FFMPEG_INCLUDE_DIRS})
#target_link_directories(TestVideoRecv- PRIVATE ${FFMPEG_LIBRARY_DIRS})
#target_link_libraries(TestVideoRecv- PRIVATE ${FFMPEG_LIBRARIES} Qt::Widgets)

include_directories(..)
add_subdirectory(sti)

add_executable(fftest2 test_encode.cpp)
#target_link_libraries(fftest2 avcodec avformat avutil avfilter)
target_include_directories(fftest2 PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(fftest2  PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(fftest2  PRIVATE ${FFMPEG_LIBRARIES})

add_executable(testgrab test_grab.cpp)
#target_link_libraries(testgrab sti avdevice avcodec avformat avutil swscale )
target_include_directories(testgrab PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories   (testgrab  PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries     (testgrab  PRIVATE ${FFMPEG_LIBRARIES} sti)

add_executable(testgrab2 test_grab2.cpp)
#target_link_libraries(testgrab2 avdevice avcodec avformat avutil swscale )
target_include_directories(testgrab2 PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories   (testgrab2  PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries     (testgrab2  PRIVATE ${FFMPEG_LIBRARIES})


add_executable(test_recv_fmt1 test_recv_fmt1.cpp)
#target_link_libraries(test_recv_fmt1 PRIVATE sti)
target_include_directories(test_recv_fmt1 PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories   (test_recv_fmt1  PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries     (test_recv_fmt1  PRIVATE ${FFMPEG_LIBRARIES} sti)

#add_subdirectory(VideoRecvTest2)

add_executable(test_ts_decode test_ts_decode.cpp)
#target_link_libraries(test_ts_decode PRIVATE Qt::Gui avcodec avformat avutil swscale )
target_include_directories(test_ts_decode PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories   (test_ts_decode  PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries     (test_ts_decode  PRIVATE ${FFMPEG_LIBRARIES} Qt::Gui)

add_executable(TestVideoSend VideoSendTest.h VideoSendTest.cpp VideoSendTest.ui VideoSampleDialog.h VideoSampleDialog.cpp)
#target_link_libraries(TestVideoSend sti avcodec avformat avutil avdevice swscale Qt::Widgets Qt::Network)
target_include_directories(TestVideoSend PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories   (TestVideoSend  PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries     (TestVideoSend  PRIVATE ${FFMPEG_LIBRARIES} sti Qt::Widgets Qt::Network)

file(GLOB_RECURSE SRCS "VideoRecvTest2/*.cpp" "VideoRecvTest2/*.h" "VideoRecvTest2/*.ui")
add_executable(TestVideoRecv ${SRCS} logo.rc logo.qrc)
#target_link_libraries(TestVideoRecv PRIVATE Qt::Widgets sti avcodec avformat avutil swscale )
target_include_directories(TestVideoRecv PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories   (TestVideoRecv  PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries     (TestVideoRecv  PRIVATE ${FFMPEG_LIBRARIES} sti Qt::Widgets Qt::Network)

add_executable(test_grab3 test_grab3.cpp)
target_link_libraries(test_grab3 PRIVATE ${FFMPEG_LIBRARIES})

add_executable(test_grab4 test_grab4.cpp)
target_link_libraries(test_grab4 PRIVATE ${FFMPEG_LIBRARIES})
