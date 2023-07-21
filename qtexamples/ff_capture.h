#pragma once

#include "ffmpeg.hpp"
#include <array>
#include <format>
#include <mutex>
#include <string_view>

class ff_capture
{
public:
    ff_capture(std::array<int, 4> rect, int fps, std::string_view device = "desktop", std::string_view input = "gdigrab")
    {
        auto [x, y, width, height] = rect;
        auto in_fmt = av_find_input_format(input.data());
        REQUIRE_PTR(in_fmt, "can not find input format {}", input);

        AVDictionary *dict{ nullptr };
        av_dict_set_int(&dict, "framerate", fps, 0);
        av_dict_set_int(&dict, "draw_mouse", 0, 0);
        av_dict_set_int(&dict, "offset_x", x, 0);
        av_dict_set_int(&dict, "offset_y", y, 0);
        av_dict_set(&dict, "video_size", std::format("{}x{}", width, height).c_str(), 0);
        auto ret = avformat_open_input(&fmt_ctx_, device.data(), in_fmt, &dict);
        av_dict_free(&dict);
        REQUIRE_RET(ret);

        video_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        REQUIRE_RET(video_index_);

        video_stream_ = fmt_ctx_->streams[video_index_];
        auto picture_decodec = avcodec_find_decoder(video_stream_->codecpar->codec_id);
        REQUIRE_PTR(picture_decodec, "can not find decoder {}", (int)video_stream_->codecpar->codec_id);
        picture_avctx_ = avcodec_alloc_context3(picture_decodec);
        ret = avcodec_parameters_to_context(picture_avctx_, video_stream_->codecpar);
        ret = avcodec_open2(picture_avctx_, picture_decodec, nullptr);
        REQUIRE_RET(ret);

        sws_ctx_ = sws_getContext(width, height, AV_PIX_FMT_BGRA, width, height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
    }

    ~ff_capture()
    {
        if (!interrupted_)
        {
            stop();
        }

        while (bmp_count_ > 0)
        {
        }
        sws_freeContext(sws_ctx_);
        avcodec_free_context(&picture_avctx_);
        avformat_close_input(&fmt_ctx_);
        avformat_free_context(fmt_ctx_);
    }

public:
    void on_bmp_packet(const ff_packet_callback &func)
    {
        bmp_func_ = func;
    }

    void on_yuv_frame(const ff_frame_callback &func)
    {
        yuv_func_ = func;
    }

    void run()
    {
        auto packet = av_packet_alloc();
        auto frame = av_frame_alloc();
        while (!interrupted_)
        {
            auto ret = av_read_frame(fmt_ctx_, packet);
            if (ret < 0) continue;
            if (bmp_func_ != nullptr) bmp_func_(packet);

            ret = avcodec_send_packet(picture_avctx_, packet);
            if (ret < 0)
            {
                av_packet_unref(packet);
                continue;
            }

            while (true)
            {
                ret = avcodec_receive_frame(picture_avctx_, frame);
                if (ret < 0) break;

                auto yuv = ff_alloc_picture(AV_PIX_FMT_YUV420P, frame->width, frame->height);
                sws_scale(sws_ctx_, frame->data, frame->linesize, 0, frame->height, yuv->data, yuv->linesize);
                //yuv->time_base = packet->time_base;
                // yuv->pts = packet->pts;
                // yuv->pkt_dts = packet->dts;
                yuv->pts = bmp_count_;
                yuv->pkt_dts = yuv->pts;
                if (yuv_func_ != nullptr) yuv_func_(yuv);
                av_frame_free(&yuv);
            }

            av_packet_unref(packet);
            bmp_count_++;
        }
        av_frame_free(&frame);
        av_packet_free(&packet);
        bmp_count_ = 0;
    }

    void stop()
    {
        interrupted_ = true;
        avcodec_send_packet(picture_avctx_, nullptr);
    }

private:
    AVFormatContext *fmt_ctx_{ nullptr };
    int video_index_{ -1 };
    AVStream *video_stream_{ nullptr };
    AVCodecContext *picture_avctx_{ nullptr };
    SwsContext *sws_ctx_{ nullptr };
    size_t bmp_count_{ 0 };
    std::atomic<bool> interrupted_{ false };
    ff_frame_callback yuv_func_{ nullptr };
    ff_packet_callback bmp_func_{ nullptr };
};
