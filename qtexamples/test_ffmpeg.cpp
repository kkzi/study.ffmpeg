
#include "fftools/libffmpeg.h"

int main(int argc, char **argv)
{
    // return ffmpeg_exec({
    //    "-f",
    //    "gdigrab",
    //    "-r", "1",
    //    "-video_size",
    //    "800x400",
    //    "-i",
    //    "desktop",
    //    "-an",
    //    "-c:v",
    //    "h264_mf",
    //    "-f",
    //    "mpegts",
    //    "udp://127.0.0.1:2234",
    //});
    // ff_grab_options opts;
    // opts.offset_x = 0;
    // opts.offset_y = 0;
    // opts.width = 400;
    // opts.height = 200;
    // return ff_grab(opts, "udp://127.0.0.1:12310");

    libffmpeg ff({
        "-f",
        "gdigrab",
        "-r",
        "1",
        "-video_size",
        "800x400",
        "-i",
        "desktop",
        "-an",
        "-c:v",
        "h264_mf",
        "-f",
        "mpegts",
        "udp://127.0.0.1:2234",
    });
    ff.start();

    //libffmpeg ff2({
    //    "-f",
    //    "gdigrab",
    //    "-r",
    //    "1",
    //    "-video_size",
    //    "800x400",
    //    "-i",
    //    "desktop",
    //    "-an",
    //    "-c:v",
    //    "h264_mf",
    //    "-f",
    //    "mpegts",
    //    "udp://127.0.0.1:2234",
    //});
    //ff2.start();

    while (true)
    {
    }
}
