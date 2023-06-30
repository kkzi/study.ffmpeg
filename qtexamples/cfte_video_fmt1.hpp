#pragma once

#include <algorithm>
#include <boost/endian/conversion.hpp>
#include <cassert>
#include <iterator>
#include <mutex>
#include <string>
#include <vector>

class cfte_video_fmt1
{
public:
    struct options
    {
        int channels{ 1 };
        bool bigendian{ false };
        uint32_t syncword{ 0xFE6B2840 };
        uint16_t sfid_min{ 0 };
        uint16_t sfid_max{ 31 };
        uint16_t frame_len{ 512 };
        uint16_t reserved_len{ 2 };
    };

public:
    cfte_video_fmt1(const options &opts)
        : opts_(opts)
    {
        assert(opts_.sfid_max > opts_.sfid_min);
        assert(opts_.frame_len > sizeof(opts_.syncword) + sizeof(sfid_) + opts_.reserved_len + sizeof(uint16_t) * opts_.channels);
        sfid_count_ = opts_.sfid_max + 1 - opts_.sfid_min;

        minor_frame_payload_.resize(opts_.channels);
        for (auto i = 0; i < opts_.channels; ++i)
        {
            channs.emplace_back(std::make_unique<channel_buffer>());
            channs[i]->buffer.reserve(0xffff);
            minor_frame_payload_[i].reserve(opts_.frame_len / opts_.channels);
        }
    }

    ~cfte_video_fmt1()
    {
    }

public:
    void push_channel_packet(int channel, uint8_t *buf, size_t len)
    {
        auto &chan = channs[channel];
        std::scoped_lock lock(chan->mutex);
        std::copy(buf, buf + len, std::back_inserter(chan->buffer));
        chan->buffer_len = chan->buffer.size();
    }

    std::vector<uint8_t> make_sub_frame()
    {
        using namespace boost::endian;

        sfid_ = (sfid_ % sfid_count_) + opts_.sfid_min;

        std::vector<uint8_t> frame(opts_.frame_len);
        auto ptr = frame.data();
        size_t offset = 0;
        store_big_u32(ptr, opts_.syncword);
        offset += sizeof(opts_.syncword);
        store_big_u16(ptr + offset, sfid_++);
        offset += sizeof(sfid_);
        offset += opts_.reserved_len;

        auto len = opts_.frame_len - offset;
        auto bytes_per_channel = len / opts_.channels;
        auto bytes_per_block = opts_.channels * sizeof(uint16_t);

        for (auto i = 0; i < opts_.channels; ++i)
        {
            auto &chan = channs[i];
            if (chan->buffer_len < bytes_per_channel)
            {
                // idle frame
                auto data = (uint16_t *)(ptr + offset);
                std::fill(data, data + len / 2, 0xFADE);
                return frame;
            }
            else if (minor_frame_payload_[i].size() != bytes_per_channel)
            {
                std::scoped_lock lock(chan->mutex);
                minor_frame_payload_[i].assign(chan->buffer.begin(), chan->buffer.begin() + bytes_per_channel);
                chan->buffer.erase(chan->buffer.begin(), chan->buffer.begin() + bytes_per_channel);
                chan->buffer_len = chan->buffer.size();
            }
        }

        for (auto i = 0; i < len; i += 2)
        {
            auto channel = (i / 2) % opts_.channels;
            auto begin = (unsigned char *)minor_frame_payload_[channel].data() + (i / bytes_per_block) * 2;
            if (opts_.bigendian)
            {
                std::copy(begin, begin + 2, frame.data() + offset + i);
            }
            else
            {
                std::reverse_copy(begin, begin + 2, frame.data() + offset + i);
            }
        }
        for (auto &chan_ts : minor_frame_payload_)
        {
            chan_ts.clear();
        }
        return frame;
    }

private:
    options opts_;
    uint16_t sfid_count_{ 0 };
    uint16_t sfid_{ 0 };

    struct channel_buffer
    {
        std::mutex mutex;
        std::vector<uint8_t> buffer;
        std::atomic<size_t> buffer_len{ 0 };
    };
    std::vector<std::unique_ptr<channel_buffer>> channs;
    std::vector<std::vector<uint8_t>> minor_frame_payload_;
};
