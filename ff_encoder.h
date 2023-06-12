#pragma once

#include "ffmpeg.hpp"
#include <format>
#include <string_view>

class ff_encoder
{
public:
    ff_encoder(std::string_view fmtname, std::string_view filename, int width, int height, int fps)
    {
        auto codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        REQUIRE_PTR(codec, "find encoder {} failed", (int)AV_CODEC_ID_H264);

        enc_ctx_ = avcodec_alloc_context3(codec);
        REQUIRE_PTR(enc_ctx_, "alloc context failed");

        enc_ctx_->bit_rate = 400000;
        enc_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
        enc_ctx_->width = width;
        enc_ctx_->height = height;
        enc_ctx_->time_base = AVRational{ 1, fps };
        enc_ctx_->framerate = AVRational{ fps, 1 };
        enc_ctx_->gop_size = fps;
        enc_ctx_->max_b_frames = 1;

        if (codec->id == AV_CODEC_ID_H264)
        {
            av_opt_set(enc_ctx_->priv_data, "preset", "superfast", 0);
            av_opt_set(enc_ctx_->priv_data, "tune", "zerolatency", 0);
        }

        auto ret = avcodec_open2(enc_ctx_, codec, NULL);
        REQUIRE_RET(ret);

        // mux
        avformat_alloc_output_context2(&fmt_ctx_, nullptr, fmtname.data(), filename.data());
        if (auto nofile = fmt_ctx_->flags & AVFMT_NOFILE; nofile)
        {
        }
        else if (!filename.empty())
        {
            ret = avio_open(&fmt_ctx_->pb, filename.data(), AVIO_FLAG_WRITE);
            REQUIRE_RET(ret);
        }

        auto video_stream_ = avformat_new_stream(fmt_ctx_, codec);
        assert(video_stream_);
        video_stream_->time_base = enc_ctx_->time_base;

        if (enc_ctx_->flags & AVFMT_GLOBALHEADER) enc_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        ret = avcodec_parameters_from_context(video_stream_->codecpar, enc_ctx_);
        REQUIRE_RET(ret);

        ret = avformat_write_header(fmt_ctx_, nullptr);
        REQUIRE_RET(ret);
    }

    ~ff_encoder()
    {
        encode(nullptr);
        av_write_trailer(fmt_ctx_);
        avcodec_free_context(&enc_ctx_);
    }

public:
    // h264
    void on_packet(const ff_packet_callback &func)
    {
        callback_ = func;
    }

    void encode(AVFrame *frame)
    {
        auto arr = ff_encode(enc_ctx_, frame);
        for (auto &pkt : arr)
        {
            pkt->pts = av_rescale_q(pkt->pts, enc_ctx_->time_base, fmt_ctx_->streams[0]->time_base);
            pkt->dts = pkt->pts;
            // packet->pos = -1;

            if (callback_) callback_(pkt);

            av_interleaved_write_frame(fmt_ctx_, pkt);
            av_packet_free(&pkt);
        }
    }

private:
    AVFormatContext *fmt_ctx_{ nullptr };
    AVCodecContext *enc_ctx_{ nullptr };
    ff_packet_callback callback_{ nullptr };
};
