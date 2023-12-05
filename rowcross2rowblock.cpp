#include "qtexamples/VideoRecv/SplitFrame.h"
#include <boost/endian/conversion.hpp>
#include <format>
#include <fstream>
#include <string>
#include <vector>

using namespace std;
using namespace boost::endian;

void split_row_cross()
{
    std::ifstream in(L"D:/Project/SAC ���/��Ƶ����/��������/3·����10M��ǰ2��", std::ios::binary);

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

void split_col_cross()
{
    std::ifstream in(L"D:/Project/SAC ���/��Ƶ����/��������/3·��Ƶ���10M", std::ios::binary);

    std::vector<std::ofstream> outputs;
    for (auto i = 0; i < 3; ++i)
    {
        outputs.emplace_back(std::format("output_2{}.ts", i), std::ios::binary);
    }
    while (in.good())
    {
        std::string frame(520, 0);
        in.read(frame.data(), frame.size());

        auto offset = 8 + 4 + 2 + 2;
        for (auto i = offset; i < frame.size(); i += 2)
        {
            auto channel = (i - offset) % 3;
            // outputs[channel].write(frame.data() + i, 2);
            outputs[channel].write(frame.data() + i + 1, 1);
            outputs[channel].write(frame.data() + i, 1);
        }
    }
}

int main(int argc, char **argv)
{
    // split_row_cross();
    split_col_cross();
}
