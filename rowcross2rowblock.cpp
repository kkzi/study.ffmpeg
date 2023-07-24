#include <boost/endian/conversion.hpp>
#include <fstream>
#include <string>
#include <vector>

using namespace std;
using namespace boost::endian;

int main(int argc, char **argv)
{
    std::ifstream in(L"D:/Project/SAC 沈飞/视频问题/测试数据/3路横排10M空前2行", std::ios::binary);
    std::ofstream out(L"output.bin", std::ios::binary);

    std::vector<std::string> matrix(32);

    int count = 0;
    while (in.good())
    {
        std::string frame(520, 0);
        in.read(frame.data(), frame.size());
        auto id = load_big_u16((unsigned char *)frame.data() + 12);

        if (id < 2)
        {
            matrix[id] = frame;
            count++;
            continue;
        }
        // auto channel = (id - 2) % 3;
        auto index = ((id - 2) % 3) * 10 + 2 + std::floor((id - 2) / 3);
        frame[13] = index;
        matrix[index] = frame;
        if (++count == 32)
        {
            for (auto &&row : matrix)
            {
                out.write(row.data(), row.size());
            }
            out.flush();
            matrix.clear();
            matrix.resize(32);
            count = 0;
        }
    }
    for (auto i = 0; i < count; ++i)
    {
        auto frame = matrix[i];
        out.write(frame.data(), frame.size());
    }
    out.close();
}
