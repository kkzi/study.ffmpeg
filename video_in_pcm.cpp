#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fmt/format.h>
#include <fmt/std.h>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string_view>
#include <vector>

struct Range
{
    size_t begin{ 0 };
    size_t end{ 0 };
};

struct PcmFrame
{
    size_t major_len{ 0 };  // 1-M
    size_t minor_len{ 0 };  // 0-N
    size_t sfid_pos{ 0 };
    size_t sfid_len{ 0 };
    bool sfid_use_bigendian{ true };
};

struct PcmFileConfig
{
    std::string filename;
    size_t file_offset{ 0 };
    size_t frame_offset{ 0 };
    PcmFrame pcm;
    std::map<size_t, std::vector<Range>> sfid2ranges;
};

class PcmFileReader
{
public:
    PcmFileReader(PcmFileConfig cfg)
        : cfg_(std::move(cfg))
    {
    }

public:
    void read(const std::function<void(std::string_view)> &callback)
    {
        size_t major_bytes = cfg_.pcm.major_len * cfg_.pcm.minor_len;
        size_t minor_bytes_with_offset = cfg_.frame_offset + cfg_.pcm.minor_len;
        size_t major_bytes_with_offset = minor_bytes_with_offset * cfg_.pcm.major_len;
        auto buffer_len = std::max(major_bytes_with_offset, cfg_.file_offset);
        std::vector<char> buffer;
        buffer.reserve(buffer_len);
        std::vector<char> major;
        major.reserve(major_bytes);

        std::ifstream input(cfg_.filename, std::ios::binary);
        while (input.good())
        {
            buffer.resize(buffer_len);
            input.read(buffer.data(), cfg_.file_offset);
            input.read(buffer.data(), major_bytes_with_offset);
            if (input.gcount() != major_bytes_with_offset)
            {
                fmt::println("finished");
                break;
            }
            for (auto i = 0; i < cfg_.pcm.major_len; ++i)
            {
                auto line = read_embed_frame({ buffer.data() + i * minor_bytes_with_offset + cfg_.frame_offset, cfg_.pcm.minor_len });
                std::copy(line.begin(), line.end(), std::back_inserter(major));
            }
            callback({ major.data(), major.size() });
            major.clear();
        }
    }

private:
    std::vector<unsigned char> read_embed_frame(std::string_view frame)
    {
        std::vector<unsigned char> line;
        auto sfid = read_sfid(frame);
        if (!cfg_.sfid2ranges.contains(sfid))
        {
            return {};
        }

        auto ptr = frame.data();
        for (auto &&it : cfg_.sfid2ranges.at(sfid))
        {
            std::copy(ptr + it.begin, ptr + it.end + 1, std::back_inserter(line));
        }

        fmt::println("sfid={:02X}, embed_bytes={}", sfid, line.size());

        return line;
    }

    uint64_t read_sfid(std::string_view frame)
    {
        auto value = *(uint16_t *)(frame.data() + cfg_.pcm.sfid_pos);
        auto ptr = (uint8_t *)&value;
        if (cfg_.pcm.sfid_use_bigendian)
        {
            std::reverse(ptr, ptr + sizeof(value));
        }
        return value;
    }

private:
    PcmFileConfig cfg_;
};

PcmFileConfig make_file0()
{
    return PcmFileConfig{ .filename = "D:/Project/SAC 沈飞/视频格式3/ZN2023-12-01-px",
        .file_offset = 0,
        .frame_offset = 8,
        .pcm = { 16, 256, 4, 16 },
        .sfid2ranges = {
            { 3, { { 82, 255 } } },
            { 4, { { 82, 255 } } },

            { 5, { { 58, 255 } } },
            { 6, { { 58, 255 } } },
            { 7, { { 58, 255 } } },
            { 8, { { 58, 255 } } },

            { 9, { { 68, 255 } } },
            { 10, { { 68, 255 } } },
            { 11, { { 68, 255 } } },
            { 12, { { 68, 255 } } },

            { 13, { { 82, 255 } } },
            { 14, { { 82, 255 } } },
            { 15, { { 82, 255 } } },
        } };
}

auto make_file1()
{
    return PcmFileConfig{ .filename = "D:/Project/SAC 沈飞/视频格式3/ZN2023-12-01-dx",
        .file_offset = 0,
        .frame_offset = 8,
        .pcm = { 8, 256, 4, 16 },
        .sfid2ranges = {
            { 0, { { 6, 255 } } },
            { 1, { { 6, 255 } } },
            { 2, { { 6, 255 } } },
            { 3, { { 6, 255 } } },
        } };

    // return PcmFileConfig{ .filename = "D:/Project/SAC 沈飞/视频格式3/ZN2023-12-01-dx",
    //    .file_offset = 0,
    //    .frame_offset = 8,
    //    .pcm = { 8, 256, 4, 16 },
    //    .sfid2ranges = {
    //        { 4, { { 6, 255 } } },
    //        { 5, { { 6, 255 } } },
    //        { 6, { { 6, 255 } } },
    //        { 7, { { 6, 255 } } },
    //    } };
}

int main(int argc, char **argv)
{
    PcmFileReader fr(make_file0());
    std::ofstream output("embedded.ts", std::ios::binary);
    fr.read([&output](auto &&frame) {
        output.write(frame.data(), frame.size());
        output.flush();
    });
}
