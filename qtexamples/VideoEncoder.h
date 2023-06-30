#pragma once

#include <string>
#include <functional>
#include <memory>

struct AVFrame;
struct AVPacket;

class VideoEncoder
{
public:
    struct Options
    {
        std::string codec_name{ "h264_mf" };
        int framerate{ 25 };
        int bitrate{ 400000 };
        int width{ 400 };
        int height{ 240 };
    };

public:
    VideoEncoder(std::function<void(AVPacket*)>, Options opts = {});
    ~VideoEncoder();

public:
    void encode(AVFrame* frame);

private:
    void init(AVFrame *frame);

private:
    std::function<void(AVPacket*)> callback_{ nullptr };
    Options opts_;
    struct Impl;
    std::shared_ptr<Impl> impl_{ nullptr };
};

