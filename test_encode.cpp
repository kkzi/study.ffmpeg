#include "ff_capture.h"
#include "ff_encoder.h"
#include "ffmpeg.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace std::literals;

static int CHANNEL_COUNT = 1;
static int FRAMERATE = 10;
static int WIDTH = 600;
static int HEIGHT = 360;

void test_cap_rtp(int index)
{
    auto url = std::format("rtp://234.1.1.1:{}", 12300 + index * 10);
    printf("rtp url: %s\n", url.c_str());
    ff_encoder rtpts_enc("rtp_mpegts", url, WIDTH, HEIGHT, FRAMERATE);
    ff_capture cap({ 0, index * HEIGHT, WIDTH, HEIGHT }, FRAMERATE);
    cap.on_yuv_frame([&rtpts_enc](auto &&yuv) {
        rtpts_enc.encode(yuv);
    });
    cap.run();
}

void test_cap_rtp_thread(int n)
{
    std::vector<std::thread> threads;
    for (auto i : std::ranges::views::iota(0, n))
    {
        threads.emplace_back([i] {
            test_cap_rtp(i);
        });
    }
    for (auto &trd : threads)
    {
        trd.join();
    }
}

int main(int argc, char **argv)
{
    avdevice_register_all();

    // test_cap_rtp(0);
    test_cap_rtp_thread(2);
}
