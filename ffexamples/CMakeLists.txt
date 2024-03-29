find_package(SDL2 CONFIG REQUIRED)

add_executable(avio_http_serve_files "avio_http_serve_files.c")
target_link_libraries(avio_http_serve_files PRIVATE
    ${FFMPEG_LIBRARIES}
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

add_executable(demuxing_decoding demuxing_decoding.c)
target_link_libraries(demuxing_decoding PRIVATE ${FFMPEG_LIBRARIES}
)
