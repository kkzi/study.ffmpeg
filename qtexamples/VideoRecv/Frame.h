#pragma once

#include <vector>

struct Frame
{
    size_t index{ 0 };
    double time{ 0 };
    std::vector<uint8_t> payload;
    size_t offset{ 0 };
};
