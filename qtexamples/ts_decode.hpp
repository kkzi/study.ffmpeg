#pragma once

#include "ffmpeg.hpp"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <format>
#include <fstream>
#include <mutex>
#include <vector>

class ts_decode
{
public:
    struct options
    {
        // int width{ 400 };
        // int height{ 200 };
        AVPixelFormat pix_fmt{ AV_PIX_FMT_RGB32 };
    };

public:
    ts_decode(int index, const options &opts)
        : opts_(opts)
        , index_(index)
    {
        stream_.reserve(0xFFFF);
    }

    ~ts_decode()
    {
        interrupted_ = true;
        cond_.notify_all();
        if (thread_.joinable())
        {
            thread_.join();
        }

        av_packet_free(&packet_);
        av_frame_free(&frame_);

        if (avio_ctx_)
        {
            av_free(avio_ctx_->buffer);
            // avio_close(fmt_ctx_->pb);
        }
        avcodec_free_context(&av_ctx_);
        avformat_close_input(&fmt_ctx_);
        avformat_free_context(fmt_ctx_);
    }

public:
    void start(const std::function<void(uint8_t *, int)> &callback)
    {
        thread_ = std::thread([this, callback] {
            init();
            // std::ofstream out("www_recv_0.yuv", std::ios::binary | std::ios::trunc);
            while (!interrupted_)
            {
                while (av_read_frame(fmt_ctx_, packet_) >= 0)
                {
                    if (packet_->stream_index == video_index_)
                    {
                        auto frames = ff_decode(av_ctx_, packet_);
                        std::for_each(frames.begin(), frames.end(), [this, /*&out,*/ callback](AVFrame *f) {
                            printf("[%x] %d pts %lld\n", GetCurrentThreadId(), index_, f->pts);
                            // if (f->pict_type == AV_PICTURE_TYPE_I)
                            //{
                            //    avcodec_flush_buffers(av_ctx_);
                            //}
                            // ff_save_yuv_file(out, f);
                            auto width = f->width;
                            auto height = f->height;
                            uint8_t *pixels[4]{ 0 };
                            int pitch[4]{ 0 };
                            // av_image_copy(raw_buffer, raw_linesize, (const uint8_t **)frame->data, frame->linesize, av_ctx_->pix_fmt, width, height);
                            sws_ctx_ = sws_getCachedContext(sws_ctx_, width, height, av_ctx_->pix_fmt, width, height, AV_PIX_FMT_BGRA, SWS_BICUBIC, 0, 0, 0);
                            av_image_alloc(pixels, pitch, width, height, AV_PIX_FMT_BGRA, 1);
                            sws_scale(sws_ctx_, f->data, f->linesize, 0, height, pixels, pitch);
                            callback(pixels[0], pitch[0]);
                            av_freep(&pixels[0]);
                            av_frame_free(&f);
                        });
                    }
                    av_packet_unref(packet_);
                    if (interrupted_) return;
                }
            }
        });
    }

    void push_bytes(const std::vector<uint8_t> &bytes)
    {
        if (interrupted_)
        {
            return;
        }
        if (bytes.empty())
        {
            return;
        }
        std::scoped_lock lock(mutex_);
        if (stream_.size() >= 0xffff)
        {
            stream_.clear();
        }
        std::copy(bytes.begin(), bytes.end(), std::back_inserter(stream_));
        cond_.notify_all();
    }

private:
    void init()
    {
        fmt_ctx_ = avformat_alloc_context();
        assert(fmt_ctx_);
        fmt_ctx_->flags |= AVFMT_FLAG_CUSTOM_IO;
        fmt_ctx_->flags |= AVFMT_FLAG_FLUSH_PACKETS;

        auto buffer_len = 0xffff;
        auto avio_buffer = (unsigned char *)av_malloc(buffer_len);
        avio_ctx_ = avio_alloc_context(
            avio_buffer, buffer_len, 0, this,
            [](void *opaque, uint8_t *buf, int len) -> int {
                return ((ts_decode *)opaque)->read_stream(buf, len);
            },
            nullptr, nullptr);
        assert(avio_ctx_);
        fmt_ctx_->pb = avio_ctx_;

        int ret = avformat_open_input(&fmt_ctx_, "", nullptr, nullptr);
        if (interrupted_) return;
        auto err = ff_err2str(ret);
        assert(ret >= 0);

        ret = avformat_find_stream_info(fmt_ctx_, nullptr);
        assert(ret >= 0);

        video_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (interrupted_) return;
        if (video_index_ < 0)
        {
            throw std::runtime_error("Couldn't find video stream.");
        }

        video_stream_ = avformat_new_stream(fmt_ctx_, nullptr);
        assert(video_stream_);
        video_stream_ = fmt_ctx_->streams[video_index_];

        auto codec = avcodec_find_decoder(video_stream_->codecpar->codec_id);
        assert(codec);

        av_ctx_ = avcodec_alloc_context3(codec);
        assert(av_ctx_);

        ret = avcodec_parameters_to_context(av_ctx_, video_stream_->codecpar);
        assert(ret >= 0);

        av_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
        av_ctx_->pkt_timebase = video_stream_->time_base;

        ret = avcodec_open2(av_ctx_, codec, nullptr);
        assert(ret >= 0);

        packet_ = av_packet_alloc();
        assert(packet_);

        frame_ = av_frame_alloc();
        assert(frame_);
    }

    int read_stream(uint8_t *buf, int len)
    {
        std::unique_lock lock(mutex_);
        cond_.wait(lock, [this] {
            return interrupted_ || !stream_.empty();
        });
        if (interrupted_) return -1;

        auto read = std::min<size_t>(len, stream_.size());
        std::copy_n(stream_.begin(), read, buf);
        stream_.erase(stream_.begin(), stream_.begin() + read);
        return read;
    }

private:
    int index_{ -1 };
    options opts_;

    AVFormatContext *fmt_ctx_{ nullptr };
    AVCodecContext *av_ctx_ = nullptr;
    int video_index_{ -1 };
    AVStream *video_stream_{ nullptr };
    AVFrame *frame_{ nullptr };
    AVPacket *packet_{ nullptr };
    AVIOContext *avio_ctx_{ nullptr };
    SwsContext *sws_ctx_{ nullptr };

    std::mutex mutex_;
    std::condition_variable cond_;
    std::vector<uint8_t> stream_;

    std::atomic<bool> interrupted_{ false };
    std::thread thread_;
};
