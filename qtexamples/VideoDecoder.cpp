#include "VideoDecoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>
}

#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <cassert>
#include <atomic>
#include <mutex>


static std::string ff_err2str(int ret)
{
    char text[AV_ERROR_MAX_STRING_SIZE]{ 0 };
    return av_make_error_string(text, AV_ERROR_MAX_STRING_SIZE, ret);
}

struct MediaStream {
    int index{ -1 };
    AVCodecContext* dec_ctx{ nullptr };
    size_t frame_count{ 0 };
};

struct VideoStream : public MediaStream {
    int width{ -1 };
    int height{ -1 };
    enum AVPixelFormat pixfmt { AV_PIX_FMT_NONE };

    uint8_t* raw_buffer[4] = { 0 };
    int raw_linesize[4];
    int raw_buffersize{ 0 };

    SwsContext* sws_ctx_{ nullptr };
};

using AudioStream = MediaStream;


struct VideoDecoder::Impl
{
    std::string filepath_;
    AVFormatContext* fmt_ctx_{ nullptr };
    VideoStream video_;
    AudioStream audio_;

    std::atomic<bool> interrupted_{ false };
    std::thread decode_thread_;
    std::function<bool(MediaType, uint8_t*, size_t)>&& output_{ nullptr };

    std::tuple<bool, std::string> open(MediaType mt)
    {
        if (avformat_open_input(&fmt_ctx_, filepath_.data(), NULL, NULL) < 0) {
            return { false, "Could not open source file" };
        }

        if (avformat_find_stream_info(fmt_ctx_, NULL) < 0) {
            return { false, "Could not find stream information" };
        }

        try
        {
            if (mt & MediaType::Video) {
                open_video_codec();
            }
            if (mt & MediaType::Audio) {
                open_audio_codec();
            }
        }
        catch (const std::exception& e)
        {
            return { false, e.what() };
        }

        return { true, "" };
    }

    std::tuple<bool, std::string> start_decode(const std::function<bool(MediaType, uint8_t*, size_t)>& callback)
    {
        output_ = callback;
        auto frame = av_frame_alloc();
        if (!frame) {
            return { false, "Could not allocate frame" };
        }

        auto pkt = av_packet_alloc();
        if (!pkt) {
            return { false, "Could not allocate packet" };
        }
        assert(output_);


        /* read frames from the file */
        bool ok = true;
        std::string detail;
        while (!interrupted_ && av_read_frame(fmt_ctx_, pkt) >= 0) {
            if (pkt->stream_index == video_.index && video_.dec_ctx) {
                std::tie(ok, detail) = decode_packet(video_.dec_ctx, pkt, frame);
            }
            else if (pkt->stream_index == audio_.index && audio_.dec_ctx) {
                std::tie(ok, detail) = decode_packet(audio_.dec_ctx, pkt, frame);
            }
            av_packet_unref(pkt);

            if (!ok) {
                break;
            }
        }

        /* flush the decoders */
        if (video_.dec_ctx)
            decode_packet(video_.dec_ctx, NULL, frame);
        if (audio_.dec_ctx)
            decode_packet(audio_.dec_ctx, NULL, frame);

        av_packet_free(&pkt);
        av_frame_free(&frame);

        return { ok, detail };
    }

    void open_video_codec()
    {
        open_codec_context(video_, AVMEDIA_TYPE_VIDEO);
        video_.width = video_.dec_ctx->width;
        video_.height = video_.dec_ctx->height;
        video_.pixfmt = video_.dec_ctx->pix_fmt;
        auto ret = av_image_alloc(video_.raw_buffer, video_.raw_linesize, video_.width, video_.height, video_.pixfmt, 1);
        if (ret < 0) {
            throw std::runtime_error("Could not allocate raw video buffer");
        }
        video_.raw_buffersize = ret;
    }

    void open_audio_codec()
    {
        open_codec_context(audio_, AVMEDIA_TYPE_AUDIO);
    }

    void open_codec_context(MediaStream& media, enum AVMediaType type) {
        std::string media_type = av_get_media_type_string(type);
        auto ret = av_find_best_stream(fmt_ctx_, type, -1, -1, NULL, 0);
        if (ret < 0) {
            throw std::runtime_error(std::string{ "Could not find " } + media_type + " stream");
        }
        media.index = ret;

        auto av_stream = fmt_ctx_->streams[media.index];
        auto dec = avcodec_find_decoder(av_stream->codecpar->codec_id);
        if (!dec) {
            throw std::runtime_error(std::string("Failed to find codec: ") + media_type);
        }

        media.dec_ctx = avcodec_alloc_context3(dec);
        if (!media.dec_ctx) {
            throw std::runtime_error(std::string("Failed to allocate the codec context: ") + media_type);
        }

        /* Copy codec parameters from input stream to output codec context */
        if (avcodec_parameters_to_context(media.dec_ctx, av_stream->codecpar) < 0) {
            throw std::runtime_error(std::string("Failed to copy codec parameters to decoder context: ") + media_type);
        }

        /* Init the decoders */
        if (avcodec_open2(media.dec_ctx, dec, NULL) < 0) {
            throw std::runtime_error(std::string("Failed to open codec: ") + media_type);
        }
    }

    std::tuple<bool, std::string> decode_packet(AVCodecContext* dec, AVPacket* pkt, AVFrame* frame) {
        // submit the packet to the decoder
        auto ret = avcodec_send_packet(dec, pkt);
        if (ret < 0) {
            return { false, std::string("Error submitting a packet for decoding: ") + ff_err2str(ret) };
        }

        // get all the available frames from the decoder
        while (ret >= 0) {
            if (interrupted_)
            {
                return { false, "" };
            }
            ret = avcodec_receive_frame(dec, frame);
            if (ret < 0) {
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                {
                    return { true, "" };
                }
                return { false, std::string("Error during decoding: ") + ff_err2str(ret) };
            }

            // write the frame data to output file
            if (dec->codec->type == AVMEDIA_TYPE_VIDEO)
                ret = output_video_frame(frame);
            else
                ret = output_audio_frame(frame);

            av_frame_unref(frame);
        }
        return { true ,"" };
    }

    int output_video_frame(AVFrame* frame) {
        static auto output_pixfmt = AV_PIX_FMT_RGB32;
        //static auto output_pixfmt = AV_PIX_FMT_YUV420P;
        /* copy decoded frame to destination buffer:
         * this is required since rawvideo expects non aligned data */
        av_image_copy(video_.raw_buffer, video_.raw_linesize, (const uint8_t**)(frame->data), frame->linesize, video_.pixfmt, video_.width, video_.height);

        if (output_pixfmt != video_.pixfmt)
        {
            video_.sws_ctx_ = sws_getCachedContext(video_.sws_ctx_,
                video_.width, video_.height, video_.pixfmt,
                video_.width, video_.height, output_pixfmt,
                0, NULL, NULL, NULL);

            uint8_t* pixels[4];
            int pitch[4];
            auto ret = av_image_alloc(pixels, pitch, video_.width, video_.height, output_pixfmt, 1);
            if (ret >= 0) {
                sws_scale(video_.sws_ctx_, (const uint8_t* const*)video_.raw_buffer, video_.raw_linesize,
                    0, video_.height, pixels, pitch);

                output_(MediaType::Video, pixels[0], pitch[0]);
            }
            av_freep(&pixels[0]);
        }
        else
        {
            output_(MediaType::Video, video_.raw_buffer[0], video_.raw_buffersize);
        }

        return 0;
    }

    int output_audio_frame(AVFrame* frame) {
        auto unpadded_linesize = (int64_t)frame->nb_samples * av_get_bytes_per_sample((AVSampleFormat)frame->format);
        output_(MediaType::Audio, frame->extended_data[0], unpadded_linesize);

        return 0;
    }
};

static std::once_flag flag;

VideoDecoder::VideoDecoder()
    : impl_(std::make_unique<Impl>())
{
    std::call_once(flag, [] {
        avformat_network_init();
        });
}

VideoDecoder::~VideoDecoder() {
    impl_->interrupted_ = true;
    if (impl_->decode_thread_.joinable())
    {
        impl_->decode_thread_.join();
    }
    avcodec_free_context(&impl_->video_.dec_ctx);
    avcodec_free_context(&impl_->audio_.dec_ctx);
    avformat_close_input(&impl_->fmt_ctx_);
    sws_freeContext(impl_->video_.sws_ctx_);
    av_freep(&impl_->video_.raw_buffer[0]);
}

std::tuple<bool, VideoDecoder::MediaInfo> VideoDecoder::open(std::string_view filepath, MediaType mt)
{
    impl_->filepath_ = std::move(filepath);

    auto&& [ok, detail] = impl_->open(mt);

    auto vstream = impl_->fmt_ctx_->streams[impl_->video_.index];
    auto rate = impl_->video_.dec_ctx->framerate;

    return { ok, MediaInfo{
        impl_->filepath_,
        detail,
        impl_->video_.index,
        avcodec_get_name(impl_->video_.dec_ctx->codec_id),
        (size_t)impl_->video_.dec_ctx->bit_rate,
        (double)rate.num / rate.den,
        impl_->video_.width,
        impl_->video_.height,
    } };
}

void VideoDecoder::start_decode(std::function<bool(MediaType, uint8_t*, size_t)>&& callback)
{
    impl_->decode_thread_ = std::thread([this, callback] {
        impl_->start_decode(callback);
        });
}

