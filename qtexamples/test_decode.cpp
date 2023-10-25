#include "ff_decoder.h"
#include <fstream>
#include <thread>

void decode_from_url()
{
    //ff_decoder dec("rtp://234.1.1.1:1234");
    ff_decoder dec("rtp://234.0.0.1:50000");
    dec.on_frame([](auto &&frame) {
        printf("[%x] pts %lld\n", std::this_thread::get_id(), frame->pts);
    });
    dec.run();
}

void decode_from_mem()
{
    ff_decoder dec;
    dec.on_frame([](auto &&frame) {
        printf("[%x] pts %lld\n", std::this_thread::get_id(), frame->pts);
    });

    std::thread enc_trd([&dec] {
        dec.run();
    });

    std::ifstream ts_file("input.ts", std::ios::binary);
    while (ts_file.good())
    {
        static constexpr auto len = 188;
        uint8_t frame[len]{ 0 };
        ts_file.read((char *)&frame[0], len);
        dec.push_bytes(frame, len);
    }
    enc_trd.join();
}

int main(int argc, char **argv)
{
    decode_from_url();
    // decode_from_mem();
}
