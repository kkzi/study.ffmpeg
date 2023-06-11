#pragma once

#include "ffmpeg.hpp"
#include <algorithm>
#include <format>
#include <mutex>
#include <string_view>
#include <vector>

class ff_encoder
{
public:
    ff_encoder(std::string_view filename, std::string_view fmtname, int width, int height, int fps)
    {
        avformat_alloc_output_context2(&fmt_ctx_, nullptr, fmtname.data(), filename.data());
        REQUIRE_PTR(fmt_ctx_, "open output context failed");
        // fmt_ctx_->flags |= AVFMT_FLAG_CUSTOM_IO;
        // fmt_ctx_->flags |= AVFMT_FLAG_FLUSH_PACKETS;

        // auto avio_buffer = (unsigned char *)av_malloc(0xFFFF);
        // fmt_ctx_->pb = avio_alloc_context(
        //    avio_buffer, 0xFFFF, 1, this, nullptr,
        //    [](void *opaque, uint8_t *buf, int len) -> int {
        //        return ((ff_encoder *)opaque)->update_stream(buf, len);
        //    },
        //    nullptr);
        // REQUIRE_PTR(fmt_ctx_->pb, "alloc avio context failed");

        // auto codec = avcodec_find_encoder(fmt_ctx_->oformat->video_codec);
        // auto codec = avcodec_find_encoder_by_name("h264_mf");
        auto codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        assert(codec);

        av_ctx_ = avcodec_alloc_context3(codec);
        assert(av_ctx_);
        av_ctx_->codec_id = codec->id;
        av_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;
        av_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
        av_ctx_->width = width;
        av_ctx_->height = height;
        av_ctx_->framerate = av_make_q(fps, 1);
        av_ctx_->time_base = av_inv_q(av_ctx_->framerate);
        av_ctx_->gop_size = fps;
        // av_ctx_->bit_rate = 400000;
        // av_ctx_->gop_size = 1;
        // av_ctx_->flags |= AVFMT_NOFILE;
        AVDictionary *param = 0;
        if (av_ctx_->codec_id == AV_CODEC_ID_H264)
        {
            av_dict_set(&param, "preset", "superfast", 0);
            av_dict_set(&param, "tune", "zerolatency", 0);
            av_ctx_->thread_count = 6;
            av_ctx_->thread_type = FF_THREAD_FRAME;

            // av_dict_set(&param, "profile", "main", 0);
            // av_dict_set(&param, "preset", "medium", 0);
        }

        auto ret = avcodec_open2(av_ctx_, codec, &param);
        REQUIRE_RET(ret);

        video_stream_ = avformat_new_stream(fmt_ctx_, codec);
        assert(video_stream_);
        video_stream_->time_base = av_ctx_->time_base;

        if (av_ctx_->flags & AVFMT_GLOBALHEADER) av_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        ret = avcodec_parameters_from_context(video_stream_->codecpar, av_ctx_);
        REQUIRE_RET(ret);

        if (!filename.empty())
        {
            ret = avio_open(&fmt_ctx_->pb, filename.data(), AVIO_FLAG_READ_WRITE);
            // ret = avio_open(&fmt_ctx_->pb, filename.data(), AVIO_FLAG_WRITE);
            REQUIRE_RET(ret);
        }

        ret = avformat_write_header(fmt_ctx_, nullptr);
        REQUIRE_RET(ret);
    }

    ~ff_encoder()
    {
        // flush_encoder(fmt_ctx_, 0);
        av_write_trailer(fmt_ctx_);

        // if (fmt_ctx_->pb)
        //{
        //    av_free(fmt_ctx_->pb->buffer);
        //    // avio_close(fmt_ctx_->pb);
        //}
        avcodec_free_context(&av_ctx_);
        avformat_close_input(&fmt_ctx_);
        avformat_free_context(fmt_ctx_);
    }

public:
    void on_packet(const std::function<void(uint8_t *, size_t)> &func)
    {
        callback_ = func;
    }

    void encode(AVFrame *frame)
    {
        auto ret = av_frame_make_writable(frame);
        auto &&pkgs = ff_encode(av_ctx_, frame);
        std::for_each(pkgs.begin(), pkgs.end(), [this](auto &&packet) {
            packet->stream_index = video_stream_->index;
            // if (packet->pts == AV_NOPTS_VALUE)
            {
                packet->pts = av_rescale_q(packet->pts, av_ctx_->time_base, video_stream_->time_base);
                // packet->pts = av_rescale_q(frame_count_, av_ctx_->time_base, video_stream_->time_base);
                packet->dts = packet->pts;
            }
            av_packet_rescale_ts(packet, av_ctx_->time_base, video_stream_->time_base);
            packet->pos = -1;
            av_interleaved_write_frame(fmt_ctx_, packet);
            av_packet_free(&packet);
        });
        if (!pkgs.empty())
        {
            frame_count_++;
        }
    }

private:
    int update_stream(uint8_t *buf, int len)
    {
        callback_(buf, len);
        return len;
    }

private:
    AVFormatContext *fmt_ctx_{ nullptr };
    AVCodecContext *av_ctx_ = nullptr;
    AVStream *video_stream_{ nullptr };
    size_t frame_count_{ 0 };
    std::function<void(uint8_t *, size_t)> callback_{ nullptr };
};
