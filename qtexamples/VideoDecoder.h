#pragma once


#include <functional>
#include <string_view>
#include <memory>


class VideoDecoder
{
public:
    enum MediaType {
        Video = 1,
        Audio = 1 << 1,
        //Subtitle = 1 << 2,
    };

    struct MediaInfo {
        std::string url;
        std::string detail;

        int video_channel{ -1 };
        std::string video_codac;
        size_t bit_rate;
        double fps{ 0 };
        int video_width{ 0 };
        int video_height{ 0 };
    };

public:
    VideoDecoder();
    ~VideoDecoder();

public:
    std::tuple<bool, MediaInfo> open(std::string_view filepath, MediaType mt = MediaType::Video);
    void start_decode(std::function<bool(MediaType, uint8_t*, size_t)>&& callback);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};


