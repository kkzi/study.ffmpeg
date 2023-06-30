project(VideoRecvTest2)

file(GLOB_RECURSE SRCS "*.cpp" "*.h" "*.ui")

set(CMAKE_CXX_STANDARD 20)

include_directories(..)
add_executable(${PROJECT_NAME} ${SRCS})
target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Widgets sti avcodec avformat avutil swscale )
