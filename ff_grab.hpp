#pragma once

#include "ffmpeg.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

class ff_grab
{
public:
    struct options
    {
        int x{ 0 };
        int y{ 0 };
        int width{ 400 };
        int height{ 200 };
        int framerate{ 25 };
        std::string device{ "gdigrab" };
        std::string input{ "desktop" };
    };

    struct status
    {
        bool ok{ true };
        std::string detail;
        uint32_t picture0_tp{ 0 };
        uint32_t grab_ok_count{ 0 };
        uint32_t grab_error_count{ 0 };
        uint32_t decode_ok_count{ 0 };
        uint32_t decode_error_count{ 0 };
    };

public:
    ff_grab(const options &opts)
        : opts_(opts)
    {
    }

    ~ff_grab()
    {
        stop();
    }

    ff_grab(ff_grab &&) = delete;
    ff_grab(const ff_grab &) = delete;
    ff_grab &operator=(ff_grab &&) = delete;
    ff_grab &operator=(const ff_grab &) = delete;

public:
    void start(const ff_frame_callback &callback)
    {
        assert(callback != nullptr);
        thread_ = std::thread([this, callback] {
            try
            {
                init();
            }
            catch (const std::exception &e)
            {
                std::scoped_lock lock(mutex_);
                status_.ok = false;
                status_.detail = e.what();
                av_log(0, AV_LOG_ERROR, "%s\n", e.what());
                return;
            }
            while (!interrupted_)
            {
                std::scoped_lock lock(mutex_);
                auto ret = av_read_frame(fmt_ctx_, packet_);
                if (status_.picture0_tp == 0)
                {
                    status_.picture0_tp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                }
                if (ret < 0)
                {
                    status_.grab_error_count++;
                    continue;
                }
                if (packet_->stream_index != video_index_)
                {
                    av_packet_unref(packet_);
                    continue;
                }
                status_.grab_ok_count++;

                auto frames = ff_decode(picture_avctx_, packet_);
                std::for_each(frames.begin(), frames.end(), [this, callback](auto &&f) {
                    auto yuv_frame = ff_alloc_picture(AV_PIX_FMT_YUV420P, opts_.width, opts_.height);
                    assert(yuv_frame);
                    auto ret = sws_scale(sws_ctx_, f->data, f->linesize, 0, f->height, yuv_frame->data, yuv_frame->linesize);
                    if (ret >= 0)
                    {
                        yuv_frame->pts = picture_count_;
                        yuv_frame->pkt_dts = picture_count_;
                        callback(yuv_frame);
                    }
                    av_frame_free(&yuv_frame);
                    av_frame_free(&f);
                });
                if (frames.empty())
                {
                    status_.decode_error_count++;
                }
                else
                {
                    status_.decode_ok_count++;
                }

                picture_count_++;
                av_packet_unref(packet_);
            }
        });
    }

    ff_grab::status get_status()
    {
        std::scoped_lock lock(mutex_);
        return status_;
    }

private:
    void init()
    {
        avdevice_register_all();

        fmt_ctx_ = avformat_alloc_context();
        auto input_fmt = av_find_input_format(opts_.device.c_str());
        if (input_fmt == nullptr)
        {
            throw std::runtime_error("Couldn't find gdigrab.");
        }

        AVDictionary *dict{ nullptr };
        av_dict_set_int(&dict, "framerate", opts_.framerate, 0);
        av_dict_set_int(&dict, "draw_mouse", 0, 0);
        av_dict_set_int(&dict, "offset_x", opts_.x, 0);
        av_dict_set_int(&dict, "offset_y", opts_.y, 0);
        av_dict_set(&dict, "video_size", std::format("{}x{}", opts_.width, opts_.height).c_str(), 0);

        auto ret = avformat_open_input(&fmt_ctx_, opts_.input.c_str(), input_fmt, &dict);
        av_dict_free(&dict);
        if (ret != 0)
        {
            throw std::runtime_error("Couldn't open input stream.");
        }

        // if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0)
        //{
        //    throw std::runtime_error("can't find stream information.");
        //}

        video_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (video_index_ < 0)
        {
            throw std::runtime_error("Couldn't find video stream.");
        }

        video_stream_ = fmt_ctx_->streams[video_index_];
        assert(video_stream_);
        picture_decodec_ = avcodec_find_decoder(video_stream_->codecpar->codec_id);
        assert(picture_decodec_);
        picture_avctx_ = avcodec_alloc_context3(picture_decodec_);
        assert(picture_avctx_);
        ret = avcodec_parameters_to_context(picture_avctx_, video_stream_->codecpar);
        assert(ret >= 0);
        ret = avcodec_open2(picture_avctx_, picture_decodec_, nullptr);
        assert(ret >= 0);

        sws_ctx_ = sws_getContext(opts_.width, opts_.height, AV_PIX_FMT_BGRA, opts_.width, opts_.height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
        assert(sws_ctx_ != nullptr);

        packet_ = av_packet_alloc();
        assert(packet_ != nullptr);
    }

    void stop()
    {
        interrupted_ = true;
        if (thread_.joinable())
        {
            thread_.join();
        }
        av_packet_free(&packet_);

        avcodec_close(picture_avctx_);
        avcodec_free_context(&picture_avctx_);

        avformat_close_input(&fmt_ctx_);
        avformat_free_context(fmt_ctx_);
        sws_freeContext(sws_ctx_);
    }

private:
    options opts_;

    AVFormatContext *fmt_ctx_{ nullptr };
    AVPacket *packet_{ nullptr };
    AVCodecContext *picture_avctx_{ nullptr };
    const AVCodec *picture_decodec_{ nullptr };
    AVStream *video_stream_{ nullptr };
    SwsContext *sws_ctx_{ nullptr };
    int video_index_{ -1 };

    size_t picture_count_{ 0 };
    std::thread thread_;
    std::atomic<bool> interrupted_{ false };
    std::mutex mutex_;
    ff_grab::status status_;
};
