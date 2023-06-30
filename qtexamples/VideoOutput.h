#pragma once

#include <string>
#include <string_view>
#include <memory>

struct AVFormatContext;
struct AVFrame;
struct output_stream;

class VideoOutput
{
public:
    VideoOutput();
    ~VideoOutput();

public:
    void prepare(std::string_view input, std::string_view codec, int width, int height, int framerate);
    void write(AVFrame* frame);
    void finish();

private:
    std::string filename;
    AVFormatContext* fmtctx{ nullptr };
    const struct AVOutputFormat* outfmt{ nullptr };

    std::unique_ptr<output_stream> video{ nullptr };
    std::unique_ptr<output_stream> audio{ nullptr };
    bool encode_video{ false };
    bool encode_audio{ false };

    struct AVDictionary* opt{ nullptr };
};


