#include "ff_capture.h"
#include "ff_encoder.h"
#include "ffmpeg.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace std::literals;

static int FRAMERATE = 10;
static int CHANNEL_COUNT = 1;
static int WIDTH = 400;
static int HEIGHT = 200;

void test_encode()
{
    ff_encoder enc("mpegts", "tenc.ts", WIDTH, HEIGHT, FRAMERATE);
    enc.on_mux_packet([](auto &&buf, auto &&len) {
        // std::vector<uint8_t> packet(buf, buf + len);
    });
    std::ifstream yuvfile("ttest.yuv", std::ios::binary);
    char *buffer = new char[WIDTH * HEIGHT * 3 / 2];
    const float frame_time = 1.0f / FRAMERATE;
    int frame_count = 0;
    auto frame = ff_alloc_picture(AV_PIX_FMT_YUV420P, WIDTH, HEIGHT);
    int len = WIDTH * HEIGHT;
    while (yuvfile.good())
    {
        yuvfile.read((char *)frame->data[0], len);
        yuvfile.read((char *)frame->data[1], len / 4);
        yuvfile.read((char *)frame->data[2], len / 4);
        frame->pts = 1;
        enc.encode(frame);
    }
}

int main(int argc, char **argv)
{
    // test_encode();

    avdevice_register_all();

    std::vector<std::thread> threads;
    for (auto i = 0; i < CHANNEL_COUNT; ++i)
    {
        threads.emplace_back([i] {
            // save to file
            // ff_encoder enc("mpegts", std::format("zenc_avio{}.ts", i), WIDTH, HEIGHT, FRAMERATE);

            // ff_encoder ts_enc("mpegts", "", WIDTH, HEIGHT, FRAMERATE);
            ff_encoder rtpts_enc("rtp_mpegts", "rtp://234.1.1.1:1234", WIDTH, HEIGHT, FRAMERATE);
            // std::ofstream pktfile("zenc_func.264", std::ios::trunc | std::ios::binary);
            // enc.on_enc_packet([&pktfile](auto &&packet) {
            //    pktfile.write((char *)packet->data, packet->size);
            //    pktfile.flush();
            //});

            std::ofstream ts_file(std::format("zenc_func{}.ts", i), std::ios::trunc | std::ios::binary);
            // ts_enc.on_mux_packet([&ts_file](auto &&buf, auto &&len) {
            //    ts_file.write((char *)buf, len);
            //    ts_file.flush();
            //});

            ff_capture cap({ 0, i * HEIGHT, WIDTH, HEIGHT }, FRAMERATE);
            // size_t count = 0;
            // cap.on_bmp_packet([&count, i](auto &&bmp) {
            //    std::ofstream file(std::format("zbmp{}_{:04d}.bmp", i, count++), std::ios::binary | std::ios::trunc);
            //    file.write((char *)bmp->data, bmp->size);
            //});

            std::ofstream yuv_file(std::format("zyuv_{}.yuv", i), std::ios::binary | std::ios::trunc);
            cap.on_yuv_frame([&yuv_file, &rtpts_enc](auto &&yuv) {
                // ff_save_yuv_file(yuv_file, yuv);
                rtpts_enc.encode(yuv);
                // ts_enc.encode(yuv);
            });

            // std::thread([&cap] {
            //    std::this_thread::sleep_for(15s);
            //    cap.stop();
            //}).detach();

            cap.run();
            yuv_file.close();
            ts_file.close();
        });
    }
    for (auto &t : threads)
    {
        t.join();
    }
}
