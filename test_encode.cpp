
#include "ff_encoder.h"
#include "ffmpeg.hpp"
#include <chrono>
#include <format>
#include <fstream>
#include <string_view>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

using namespace std::literals;

static std::ofstream out("zoutput.264", std::ios::binary | std::ios::trunc);
int main(int argc, char **argv)
{
    std::ifstream yuv("zyuv_0.yuv", std::ios::binary);
    int width = 400;
    int height = 200;
    auto len = width * height;
    int count = 0;

    ff_encoder enc("rtp_mpegts", "rtp://234.0.0.1:1234", width, height, 1);
    enc.on_enc_packet([](auto &&packet) {
        out.write((char *)packet->data, packet->size);
        out.flush();
    });
    auto frame = ff_alloc_picture(AV_PIX_FMT_YUV420P, width, height);
    while (yuv.good())
    {
        fflush(stdout);

        yuv.read((char *)frame->data[0], len);
        yuv.read((char *)frame->data[1], len / 4);
        yuv.read((char *)frame->data[2], len / 4);

        frame->pts = count++;

        enc.encode(frame);
        std::this_thread::sleep_for(1s);
    }

    // enc.~ff_encoder();
    // out.close();
    return 0;
}
