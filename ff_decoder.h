#pragma once

#include "ffmpeg.hpp"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <string>
#include <string_view>

class ff_decoder
{
public:
    ff_decoder(std::string_view input)
        : input_(input)
    {
    }

    ff_decoder()
        : ff_decoder("")
    {
        stream_.reserve(0xFF'FFFF);
    }

    ~ff_decoder()
    {
        interrupted_ = true;
        cond_.notify_all();

        while (video_index_ >= 0)
        {
        }

        av_packet_free(&packet_);
        if (dec_ctx_) avcodec_free_context(&dec_ctx_);
        if (fmt_ctx_->pb)
        {
            av_free(fmt_ctx_->pb->buffer);
            avio_context_free(&fmt_ctx_->pb);
        }
        avformat_close_input(&fmt_ctx_);
        avformat_free_context(fmt_ctx_);
        sws_freeContext(sws_ctx_);
    }

public:
    void on_frame(ff_frame_callback func)
    {
        yuv_callback_ = std::move(func);
    }

    void on_bgra_picture(std::function<void(uint8_t *, size_t, int, int)> func)
    {
        bgra_callback_ = std::move(func);
    }

    void push_bytes(uint8_t *buf, size_t len)
    {
        assert(buf != nullptr);
        if (len == 0) return;

        std::scoped_lock lock(mutex_);
        std::copy(buf, buf + len, std::back_inserter(stream_));
        cond_.notify_all();
    }

    void run()
    {
        init();

        while (av_read_frame(fmt_ctx_, packet_) >= 0)
        {
            auto arr = ff_decode(dec_ctx_, packet_);
            for (auto &&f : arr)
            {
                process_yuv_frame(f);
                av_frame_free(&f);
            }
            av_packet_unref(packet_);
        }

        ff_decode(dec_ctx_, nullptr);
        video_index_ = -1;
    }

private:
    void init()
    {
        if (input_.empty())
        {
            init_avio_context();
        }

        auto ret = avformat_open_input(&fmt_ctx_, input_.c_str(), nullptr, nullptr);
        if (interrupted_) return;
        REQUIRE_RET(ret);

        ret = avformat_find_stream_info(fmt_ctx_, nullptr);
        if (interrupted_) return;
        REQUIRE_RET(ret);

        AVCodec *dec = nullptr;
        video_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
        REQUIRE_RET(video_index_);

        dec_ctx_ = avcodec_alloc_context3(dec);
        REQUIRE_PTR(dec_ctx_, "Failed to allocate codec context");

        auto video_stream = fmt_ctx_->streams[video_index_];
        ret = avcodec_parameters_to_context(dec_ctx_, video_stream->codecpar);
        REQUIRE_RET(ret);

        AVDictionary *opts = NULL;
        ret = avcodec_open2(dec_ctx_, dec, &opts);
        REQUIRE_RET(ret);

        packet_ = av_packet_alloc();
        REQUIRE_PTR(packet_, "alloc packet failed");
    }

    void init_avio_context()
    {
        fmt_ctx_ = avformat_alloc_context();
        REQUIRE_PTR(fmt_ctx_, "alloc format context failed");
        fmt_ctx_->flags |= AVFMT_FLAG_CUSTOM_IO;
        fmt_ctx_->flags |= AVFMT_FLAG_FLUSH_PACKETS;
        auto buffer_len = 0xffff;
        auto avio_buffer = (unsigned char *)av_malloc(buffer_len);
        auto avio_ctx = avio_alloc_context(
            avio_buffer, buffer_len, 0, this,
            [](void *opaque, uint8_t *buf, int len) {
                return ((decltype(this))opaque)->read_stream(buf, len);
            },
            nullptr, nullptr);
        REQUIRE_PTR(avio_ctx, "alloc avio context failed");
        fmt_ctx_->pb = avio_ctx;
    }

    int read_stream(uint8_t *buf, int len)
    {
        {
            std::unique_lock lock(mutex_);
            cond_.wait(lock, [this] {
                return interrupted_ || !stream_.empty();
            });
        }

        if (interrupted_) return -1;

        std::scoped_lock lock(mutex_);
        auto read = std::min<size_t>(len, stream_.size());
        std::copy(stream_.begin(), stream_.begin() + read, buf);
        stream_.erase(stream_.begin(), stream_.begin() + read);
        return read;
    }

    void process_yuv_frame(AVFrame *frame)
    {
        if (yuv_callback_) yuv_callback_(frame);

        if (bgra_callback_)
        {
            int width = frame->width;
            int height = frame->height;
            sws_ctx_ = sws_getCachedContext(sws_ctx_, width, height, dec_ctx_->pix_fmt, width, height, AV_PIX_FMT_BGRA, SWS_BICUBIC, nullptr, nullptr, nullptr);
            uint8_t *pixels[4]{ 0 };
            int pitch[4]{ 0 };
            av_image_alloc(pixels, pitch, width, height, AV_PIX_FMT_BGRA, 1);
            sws_scale(sws_ctx_, frame->data, frame->linesize, 0, height, pixels, pitch);
            bgra_callback_(pixels[0], pitch[0], width, height);
            av_freep(&pixels[0]);
        }
    }

private:
    std::string input_;
    ff_frame_callback yuv_callback_{ nullptr };
    std::function<void(uint8_t *, size_t, int, int)> bgra_callback_{ nullptr };

    std::atomic<bool> interrupted_{ false };
    std::mutex mutex_;
    std::vector<uint8_t> stream_;
    std::condition_variable cond_;

    AVFormatContext *fmt_ctx_{ nullptr };
    AVCodecContext *dec_ctx_{ nullptr };
    AVPacket *packet_{ nullptr };
    int video_index_{ -1 };
    SwsContext *sws_ctx_{ nullptr };
};
