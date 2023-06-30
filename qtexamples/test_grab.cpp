
#include "cfte_video_fmt1.hpp"
#include "ff_grab.hpp"
#include "sti/tm_server.h"
#include "ts_encode.hpp"
#include <algorithm>
#include <chrono>
#include <thread>

using namespace std::literals;

int main(int argc, char **argv)
{
    int framerate = 5;
    int width = 400;
    int height = 200;
    int channels = 2;

    struct test_context
    {
        ff_grab *grab;
        std::ofstream *out;
    };

    cfte_video_fmt1 fmt1({ channels, false });
    std::vector<test_context> grabs;
    for (auto i = 0; i < channels; ++i)
    {
        //auto grab = new ff_grab(ff_grab::options{ 0, i * height, width, height, framerate });
        //auto out = new std::ofstream(std::format("www_{}.yuv", i), std::ios::binary | std::ios::trunc);
        //grabs.emplace_back(grab, out);
        //grab->start([i, out, &fmt1](auto &&frame) {
        //    // save_yuv_file(*out, frame);
        //    fmt1.push_channel_packet(i, frame);
        //});
    }

    tm_server tms;
    tms.register_channel({ 0, 32, 512 });
    tms.bind(3070);
    tms.start();

    std::ofstream out("www_pcm.bin", std::ios::trunc | std::ios::binary);
    using namespace std::chrono;
    for (;;)
    {
        auto frame = fmt1.make_sub_frame();
        auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        tms.push(0, ms, std::make_shared<std::vector<uint8_t>>(frame));

        out.write((char *)frame.data(), frame.size());
        out.flush();

        std::this_thread::sleep_for(10ms);
    }

    // std::this_thread::sleep_for(30s);
    for (auto &&[g, f] : grabs)
    {
        delete g;
        delete f;
    }
}
