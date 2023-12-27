#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

// FFmpeg includes
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

class ScreenCapture
{
public:
    ScreenCapture(int x, int y, int width, int height, int fps)
        : x_(x)
        , y_(y)
        , width_(width)
        , height_(height)
        , fps_(fps)
    {
    }
    ~ScreenCapture()
    {
        sws_freeContext(sws_context_);
        avcodec_free_context(&input_codec_context_);
        av_frame_free(&frame_);
        avcodec_close(codec_context_);
        avformat_free_context(format_context_);
    }

    void initialize()
    {
        // Initialize format context
        auto *output_format = av_guess_format("mpegts", nullptr, nullptr);
        if (!output_format)
        {
            throw std::runtime_error("Unable to create output format");
        }
        format_context_ = avformat_alloc_context();
        if (!format_context_)
        {
            throw std::runtime_error("Unable to allocate output context");
        }

        // Set output format
        format_context_->oformat = output_format;

        // Create output stream
        output_stream_ = avformat_new_stream(format_context_, nullptr);
        if (!output_stream_)
        {
            throw std::runtime_error("Unable to create output stream");
        }

        // Create codec context and set codec parameters
        codec_ = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO);
        if (!codec_)
        {
            throw std::runtime_error("Unable to find codec");
        }
        codec_context_ = avcodec_alloc_context3(codec_);
        if (!codec_context_)
        {
            throw std::runtime_error("Unable to allocate codec context");
        }
        codec_context_->bit_rate = 400000;
        codec_context_->width = width_;
        codec_context_->height = height_;
        codec_context_->time_base = { 1, fps_ };
        codec_context_->gop_size = 12;
        codec_context_->max_b_frames = 2;
        codec_context_->pix_fmt = AV_PIX_FMT_YUV420P;

        // Set codec options
        AVDictionary *codec_options = nullptr;
        av_dict_set(&codec_options, "preset", "fast", 0);
        av_dict_set(&codec_options, "tune", "zerolatency", 0);
        av_opt_set(codec_context_->priv_data, "crf", "23", 0);
        av_opt_set(codec_context_->priv_data, "profile", "main", 0);
        av_opt_set(codec_context_->priv_data, "level", "4", 0);
        avcodec_open2(codec_context_, codec_, &codec_options);

        // Create frame and allocate memory
        frame_ = av_frame_alloc();
        if (!frame_)
        {
            throw std::runtime_error("Unable to allocate frame");
        }
        frame_->format = codec_context_->pix_fmt;
        frame_->width = width_;
        frame_->height = height_;
        av_frame_get_buffer(frame_, 0);

        // Set output stream parameters
        avcodec_parameters_from_context(output_stream_->codecpar, codec_context_);
        output_stream_->time_base = { 1, fps_ };
        output_stream_->codecpar->codec_tag = 0;

        // Allocate output buffer
        output_buffer_ = static_cast<uint8_t *>(av_malloc(1920 * 1080 * 3));

        // Open output URL
        if (avio_open2(&format_context_->pb, "rtp://127.0.0.1:9000", AVIO_FLAG_WRITE, nullptr, nullptr) < 0)
        {
            throw std::runtime_error("Unable to open output URL");
        }

        // Write stream header
        avformat_write_header(format_context_, nullptr);
    }

    void captureFrames()
    {
        avdevice_register_all();
        auto input_fmt = av_find_input_format("gdigrab");
        if (input_fmt == nullptr)
        {
            throw std::runtime_error("Couldn't find gdigrab.");
        }
        // Open screen capture device
        if (avformat_open_input(&input_format_context_, "desktop", input_fmt, nullptr) != 0)
        {
            throw std::runtime_error("Unable to open screen capture device");
        }

        // Find video stream info
        if (avformat_find_stream_info(input_format_context_, nullptr) < 0)
        {
            throw std::runtime_error("Unable to find input stream info");
        }
        for (unsigned int i = 0; i < input_format_context_->nb_streams; i++)
        {
            if (input_format_context_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                input_video_stream_index_ = i;
                break;
            }
        }
        if (input_video_stream_index_ < 0)
        {
            throw std::runtime_error("Unable to find video stream");
        }

        // Create and allocate input codec context
        input_codec_ = avcodec_find_decoder(input_format_context_->streams[input_video_stream_index_]->codecpar->codec_id);
        if (!input_codec_)
        {
            throw std::runtime_error("Unable to find input codec");
        }
        input_codec_context_ = avcodec_alloc_context3(input_codec_);
        if (!input_codec_context_)
        {
            throw std::runtime_error("Unable to allocate input codec context");
        }
        avcodec_parameters_to_context(input_codec_context_, input_format_context_->streams[input_video_stream_index_]->codecpar);
        avcodec_open2(input_codec_context_, input_codec_, nullptr);

        // Create and allocate scaling context
        sws_context_ = sws_getContext(input_codec_context_->width, input_codec_context_->height, input_codec_context_->pix_fmt, codec_context_->width,
            codec_context_->height, codec_context_->pix_fmt, SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!sws_context_)
        {
            throw std::runtime_error("Unable to create scaling context");
        }

        // Read and encode frames from screen capture device
        AVPacket input_packet_;
        av_init_packet(&input_packet_);
        while (av_read_frame(input_format_context_, &input_packet_) >= 0)
        {
            if (input_packet_.stream_index == input_video_stream_index_)
            {
                // Decode input frame
                AVFrame *input_frame = av_frame_alloc();
                if (!input_frame)
                {
                    throw std::runtime_error("Unable to decode input frame");
                }
                int got_input_frame = 0;
                if (avcodec_receive_frame(input_codec_context_, input_frame) < 0)
                {
                    throw std::runtime_error("Unable to receive input frame");
                }

                // Convert input frame to output frame
                sws_scale(sws_context_, input_frame->data, input_frame->linesize, 0, input_codec_context_->height, frame_->data, frame_->linesize);

                // Encode output frame and write to stream
                AVPacket output_packet_;
                av_init_packet(&output_packet_);
                output_packet_.data = output_buffer_;
                output_packet_.size = av_image_get_buffer_size(codec_context_->pix_fmt, codec_context_->width, codec_context_->height, 1);
                if (avcodec_send_frame(codec_context_, frame_) >= 0)
                {
                    while (avcodec_receive_packet(codec_context_, &output_packet_) >= 0)
                    {
                        output_packet_.stream_index = output_stream_->index;
                        av_packet_rescale_ts(&output_packet_, codec_context_->time_base, output_stream_->time_base);
                        av_write_frame(format_context_, &output_packet_);
                        av_packet_unref(&output_packet_);
                    }
                }

                // Free input resources
                av_frame_unref(input_frame);
                av_packet_unref(&input_packet_);
                av_frame_free(&input_frame);
            }
        }

        // Write stream trailer and close output URL
        av_write_trailer(format_context_);
        avio_close(format_context_->pb);
    }

private:
    int x_;
    int y_;
    int width_;
    int height_;
    int fps_;

    AVFormatContext *format_context_ = nullptr;
    AVStream *output_stream_ = nullptr;
    const AVCodec *codec_ = nullptr;
    AVCodecContext *codec_context_ = nullptr;
    uint8_t *output_buffer_ = nullptr;
    AVFrame *frame_ = nullptr;

    AVInputFormat *input_format_ = nullptr;
    AVFormatContext *input_format_context_ = nullptr;
    int input_video_stream_index_ = -1;
    const AVCodec *input_codec_ = nullptr;
    AVCodecContext *input_codec_context_ = nullptr;
    struct SwsContext *sws_context_ = nullptr;
};

int main(int argc, char **argv)
{
    try
    {
        // Initialize screen capture object
        ScreenCapture screen_capture(0, 0, 1920, 1080, 30);
        screen_capture.initialize();

        // Capture and encode screen frames
        screen_capture.captureFrames();
    }
    catch (const std::exception &e)
    {
        std::cout << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
