


add_executable(tcp_demo tcp_demo.cpp)
target_link_libraries(tcp_demo 
    PRIVATE
    ${FFMPEG_LIBRARIES}
)
