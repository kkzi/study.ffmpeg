#pragma once

#include "ffmpeg.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <string_view>

class ff_decoder
{
public:
    ff_decoder(std::string_view input, size_t probe_packet_count = 32)
        : input_(input)
        , probe_packet_count_(probe_packet_count)
    {
        stream_buffer_max_ = 188 * probe_packet_count_;
        stream_.reserve(stream_buffer_max_);
    }

    ff_decoder(size_t avio_buffer_max = 32)
        : ff_decoder("", avio_buffer_max)
    {
    }

    ~ff_decoder()
    {
        using namespace std::literals;

        auto inited = video_index_ >= 0;
        interrupted_ = true;
        cond_.notify_all();

        while (video_index_ >= 0)
        {
        }
        std::this_thread::sleep_for(0.2s);

        if (dec_ctx_)
        {
            avcodec_free_context(&dec_ctx_);
            dec_ctx_ = nullptr;
        }
        if (fmt_ctx_)
        {
            auto avio_ctx = fmt_ctx_->pb;
            if (inited)
            {
                avformat_close_input(&fmt_ctx_);
            }
            if (avio_ctx)
            {
                av_free(avio_ctx->buffer);
            }
            avio_context_free(&avio_ctx);
            // avformat_free_context(fmt_ctx_);
        }
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
        if (stream_.size() >= stream_buffer_max_)
        {
            stream_.clear();
        }
        std::copy(buf, buf + len, std::back_inserter(stream_));
        cond_.notify_all();
    }

    void run()
    {
        while (!interrupted_)
        {
            try
            {
                init();
                break;
            }
            catch (const std::exception &e)
            {
                printf("%s\n", e.what());
            }
        }

        auto pkt = av_packet_alloc();
        while (!interrupted_ and av_read_frame(fmt_ctx_, pkt) >= 0)
        {
            if (interrupted_) break;
            if (pkt->stream_index == video_index_)
            {
                auto arr = ff_decode(dec_ctx_, pkt);
                for (auto &&f : arr)
                {
                    // if (f->key_frame == 1)
                    //{
                    //    avcodec_flush_buffers(dec_ctx_);
                    //};
                    process_yuv_frame(f);
                    av_frame_free(&f);
                }
            }
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
        if (!interrupted_ and dec_ctx_)
        {
            ff_decode(dec_ctx_, nullptr);
        }
        video_index_ = -1;
    }

private:
    void init()
    {
        if (input_.empty())
        {
            fmt_ctx_ = avformat_alloc_context();
            REQUIRE_PTR(fmt_ctx_, "alloc format context failed");
            fmt_ctx_->flags |= AVFMT_FLAG_CUSTOM_IO;
            fmt_ctx_->flags |= AVFMT_FLAG_FLUSH_PACKETS;
            fmt_ctx_->flags |= AVFMT_FLAG_NONBLOCK;
            init_avio_context();
        }

        int ret = 0;
        {
            AVDictionary *opts = NULL;
            // av_dict_set_int(&opts, "resync_size", 2048, 0);
            // av_dict_set_int(&opts, "max_packet_size", 4096, 0);
            ret = avformat_open_input(&fmt_ctx_, input_.c_str(), nullptr, &opts);
            if (interrupted_) return;
            REQUIRE_RET(ret);
            fmt_ctx_->max_probe_packets = probe_packet_count_;
            fmt_ctx_->probesize = stream_buffer_max_;
            fmt_ctx_->skip_estimate_duration_from_pts = 1;
        }

        ret = avformat_find_stream_info(fmt_ctx_, nullptr);
        if (interrupted_) return;
        REQUIRE_RET(ret);

        AVCodec *dec = nullptr;
        video_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
        REQUIRE_RET(video_index_);

        av_dump_format(fmt_ctx_, video_index_, 0, 0);

        dec_ctx_ = avcodec_alloc_context3(dec);
        REQUIRE_PTR(dec_ctx_, "Failed to allocate codec context");

        auto video_stream = fmt_ctx_->streams[video_index_];
        ret = avcodec_parameters_to_context(dec_ctx_, video_stream->codecpar);
        REQUIRE_RET(ret);

        {
            AVDictionary *opts = NULL;
            ret = avcodec_open2(dec_ctx_, dec, &opts);
            REQUIRE_RET(ret);
        }
    }

    void init_avio_context()
    {
        auto avio_buffer_len = stream_buffer_max_;
        auto avio_buffer = (unsigned char *)av_malloc(avio_buffer_len);
        auto avio_ctx = avio_alloc_context(
            avio_buffer, avio_buffer_len, 0, this,
            [](void *opaque, uint8_t *buf, int len) {
                return ((decltype(this))opaque)->read_stream(buf, len);
            },
            nullptr, nullptr);
        REQUIRE_PTR(avio_ctx, "alloc avio context failed");
        // avio_ctx->max_packet_size = 10;
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
        // printf("time=%lld, pts=%lld, dts=%lld\n", time(nullptr), frame->pts, frame->pkt_dts);
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
            av_free(pixels[0]);
        }
    }

private:
    std::string input_;
    size_t probe_packet_count_{ 32 };
    size_t stream_buffer_max_{ 0 };
    ff_frame_callback yuv_callback_{ nullptr };
    std::function<void(uint8_t *, size_t, int, int)> bgra_callback_{ nullptr };

    std::atomic<bool> interrupted_{ false };
    std::mutex mutex_;
    std::vector<uint8_t> stream_;
    std::condition_variable cond_;

    AVFormatContext *fmt_ctx_{ nullptr };
    AVCodecContext *dec_ctx_{ nullptr };
    int video_index_{ -1 };
    SwsContext *sws_ctx_{ nullptr };
};
