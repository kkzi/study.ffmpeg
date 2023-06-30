#include "sti/cortex_sti_parser.h"
#include "sti/cortex_tm_client.h"
#include <boost/endian/conversion.hpp>
#include <fstream>

using namespace boost::endian;

static int channels = 4;
static int offset = 8;
static bool bigendian = true;

static std::ofstream out[]{
    std::ofstream("www_pcm_0.ts", std::ios::trunc | std::ios::binary),
    std::ofstream("www_pcm_1.ts", std::ios::trunc | std::ios::binary),
    std::ofstream("www_pcm_2.ts", std::ios::trunc | std::ios::binary),
    std::ofstream("www_pcm_3.ts", std::ios::trunc | std::ios::binary),
};

static bool is_idle_frame(uint8_t *ptr, size_t len)
{
    if (len < 8)
    {
        return true;
    }
    auto numbers = *(uint64_t *)ptr;

    return numbers == 0 || numbers == 0xFADE'FADE'FADE'FADE || numbers == 0xDEFA'DEFA'DEFA'DEFA || numbers == 0xDEAD'DEAD'DEAD'DEAD ||
           numbers == 0xADDE'ADDE'ADDE'ADDE;
}

static void output_payload(double time, const std::vector<uint8_t> &payload)
{
    auto ptr = (char *)payload.data();
    for (auto i = offset; i < payload.size(); i += 2)
    {
        auto channel = (i / 2) % channels;
        if (bigendian)
        {
            out[channel].write(ptr + i, 1);
            out[channel].write(ptr + i + 1, 1);
        }
        else
        {
            out[channel].write(ptr + i + 1, 1);
            out[channel].write(ptr + i, 1);
        }
    }
    for (auto &f : out)
    {
        f.flush();
    }
}

int main(int argc, char **argv)
{
    cortex::cortex_sti_parser parser(10 * 1024 * 1024);
    parser.set_tm_msg_callback_fun([](auto &&frame) {
        auto ptr = frame->data();
        auto time = (double)load_big_u32(ptr + 12) + (double)load_big_u32(ptr + 16) / 1e3;
        std::vector<uint8_t> payload(frame->begin() + 64, frame->end() - 4);
        if (!is_idle_frame(payload.data() + offset, payload.size()))
        {
            output_payload(time, payload);
        }
    });
    parser.start();

    cortex::crt_tm_client tmc;
    tmc.set_config({ "127.0.0.1", 3070, 0 });
    tmc.set_tm_data_callback_fun([&parser](auto &&begin, auto &&end) {
        parser.push_data(begin, end);
    });
    tmc.set_error_log_callback_fun([](auto &&msg) {
        printf("tmc error: %s\n", msg.c_str());
    });

    tmc.start();

    for (;;)
        ;
}
