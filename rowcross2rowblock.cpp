#include "qtexamples/VideoRecv/SplitFrame.h"
#include <array>
#include <boost/endian/conversion.hpp>
#include <format>
#include <fstream>
#include <string>
#include <vector>

using namespace std;
using namespace boost::endian;

void split_row_cross()
{
    std::ifstream in(L"D:/Project/SAC 沈飞/视频问题/测试数据/3路横排10M空前2行", std::ios::binary);

    std::vector<std::ofstream> outputs;
    for (auto i = 0; i < 3; ++i)
    {
        outputs.emplace_back(std::format("output_{}.ts", i), std::ios::binary);
    }
    while (in.good())
    {
        std::string frame(520, 0);
        in.read(frame.data(), frame.size());
        auto sfid = load_big_u16((unsigned char *)frame.data() + 12);
        if (sfid < 2)
        {
            continue;
        }

        auto channel = (sfid - 2) % 3;
        auto offset = 8 + 4 + 2;
        outputs[channel].write(frame.data() + offset, frame.size() - 14);
    }
}

template <size_t RowBytes, size_t Offset, size_t ChannelCount, bool BigEndian = true>
void split_col_cross(std::string file, std::string prefix)
{
    std::ifstream in(file, std::ios::binary);
    std::vector<std::ofstream> outputs;
    for (auto i = 0; i < ChannelCount; ++i)
    {
        outputs.emplace_back(std::format("{}{}.ts", prefix, i), std::ios::binary);
    }
    std::string frame(RowBytes, 0);
    while (in.good())
    {
        in.read(frame.data(), frame.size());

        for (auto i = Offset; i < frame.size(); i += 2)
        {
            auto channel = ((i - Offset) / 2) % ChannelCount;
            if (BigEndian)
            {
                // std::copy_n(frame.data() + i, 2, std::back_inserter(blocks[channel]));
                outputs[channel].write(frame.data() + i, 1);
                outputs[channel].write(frame.data() + i + 1, 1);
            }
            else
            {
                // std::reverse_copy(frame.data() + i, frame.data() + i + 2, std::back_inserter(blocks[channel]));
                outputs[channel].write(frame.data() + i + 1, 1);
                outputs[channel].write(frame.data() + i, 1);
            }
        }
    }
}

int main(int argc, char **argv)
{
    // split_row_cross();

    // ok
    // split_col_cross<520, 16, 4>("D:/Project/FTS/SAC 沈飞/视频问题/2023-05-22 4路视频-520.bin", "output_2");

    // 没画面，有信息
    // split_col_cross<520, 16, 3, true>("D:/Project/FTS/SAC 沈飞/视频问题/测试数据/3路视频大端10M", "output_3");

    // ok
    split_col_cross<268, 20, 2, false>("D:/Project/FTS/SAC 沈飞/视频问题2/20231017/20230926.bin 16x268", "output_4");

    // 无画面，无信息
    // split_col_cross<1032, 520, 2, true>("D:/Project/FTS/SAC 沈飞/视频问题2/20231017/2023-09-26SF 8x1032", "output_5");
}
