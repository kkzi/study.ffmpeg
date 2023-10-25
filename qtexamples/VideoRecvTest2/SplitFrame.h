#pragma once

#include "Frame.h"
#include <algorithm>
#include <functional>
#include <iterator>

static void split_frame_column_cross(const Frame &frame, size_t channels, bool bigendian, std::function<void(size_t, std::vector<uint8_t>)> func)
{
    int index = 0;
    auto bytesPerChannel = (frame.payload.size() - frame.offset) / channels;

    auto ptr = (char *)frame.payload.data();
    for (auto i = frame.offset; i < frame.payload.size(); i += 2)
    {
        auto index = ((i - frame.offset) / 2) % channels;
        std::vector<uint8_t> payload;
        if (bigendian)
        {
            std::copy(ptr + i, ptr + i + 2, std::back_inserter(payload));
        }
        else
        {
            std::reverse_copy(ptr + i, ptr + i + 2, std::back_inserter(payload));
        }
        func(index, std::move(payload));
    }
}
