#pragma once

#include "ffmpeg.hpp"
#include <algorithm>
#include <format>
#include <mutex>
#include <vector>

#define SAVE_TS_FILE 1

#if SAVE_TS_FILE
    #include <fstream>
#endif

class ts_encode
{
public:
    struct options
    {
        int width{ 400 };
        int height{ 200 };
        int framerate{ 20 };
        std::string codec{ "mpegts" };
    };

public:
    ts_encode(int index, const options &opts)
        : index_(index)
        , opts_(opts)
    {
    }

    ~ts_encode()
    {
        // flush_encoder(fmt_ctx_, 0);
        av_write_trailer(fmt_ctx_);

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
    void encode(AVFrame *frame)
    {
        if (fmt_ctx_ == nullptr)
        {
            init();
        }
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

    size_t stream_len() const
    {
        return stream_len_;
    }

    std::vector<uint8_t> get_bytes(size_t n)
    {
        std::scoped_lock lock(mutex_);
        if (stream_.size() < n)
            return {};
        std::vector<uint8_t> bytes(n);
        std::copy(stream_.begin(), stream_.begin() + n, bytes.begin());
        stream_.erase(stream_.begin(), stream_.begin() + n);
        stream_len_ = stream_.size();
        return bytes;
    }

private:
    void init()
    {
        avformat_alloc_output_context2(&fmt_ctx_, nullptr, opts_.codec.c_str(), "");
        assert(fmt_ctx_);
        fmt_ctx_->flags |= AVFMT_FLAG_CUSTOM_IO;
        fmt_ctx_->flags |= AVFMT_FLAG_FLUSH_PACKETS;

#if SAVE_TS_FILE
        ts_file_ = std::ofstream(std::format("www_{}.ts", index_), std::ios::binary | std::ios::trunc);
#endif

        avio_buf_ = (unsigned char *)av_malloc(0xFFFF);
        avio_ctx_ = avio_alloc_context(
            avio_buf_, 0xFFFF, 1, this, nullptr,
            [](void *opaque, uint8_t *buf, int len) -> int {
                return ((ts_encode *)opaque)->update_stream(buf, len);
            },
            nullptr);
        assert(avio_ctx_);

        fmt_ctx_->pb = avio_ctx_;
        // ts_frames_.resize(opts_.channels);

        int ret = 0;
        // auto codec = avcodec_find_encoder(fmt_ctx_->oformat->video_codec);
        // auto codec = avcodec_find_encoder_by_name("h264_mf");
        auto codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        assert(codec);

        av_ctx_ = avcodec_alloc_context3(codec);
        assert(av_ctx_);
        av_ctx_->codec_id = codec->id;
        av_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;
        av_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
        av_ctx_->width = opts_.width;
        av_ctx_->height = opts_.height;
        av_ctx_->time_base = AVRational{ 1, opts_.framerate };
        av_ctx_->framerate = AVRational{ opts_.framerate, 1 };
        av_ctx_->bit_rate = 400000;
        //av_ctx_->gop_size = opts_.framerate;
        av_ctx_->gop_size = 1;
        av_ctx_->flags |= AVFMT_NOFILE;
        if (av_ctx_->flags & AVFMT_GLOBALHEADER)
        {
            av_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        AVDictionary *param = 0;
        if (av_ctx_->codec_id == AV_CODEC_ID_H264)
        {
            // av_dict_set(&param, "preset", "medium", 0);
            av_dict_set(&param, "preset", "superfast", 0);
            av_dict_set(&param, "tune", "zerolatency", 0);  //实现实时编码
            // av_dict_set(&param, "profile", "main", 0);
        }

        ret = avcodec_open2(av_ctx_, codec, &param);
        assert(ret >= 0);

        video_stream_ = avformat_new_stream(fmt_ctx_, nullptr);
        assert(video_stream_);
        video_stream_->time_base = AVRational{ 1, opts_.framerate };
        ret = avcodec_parameters_from_context(video_stream_->codecpar, av_ctx_);
        assert(ret >= 0);

        // ret = avio_open(&fmt_ctx_->pb, "www_1.ts", AVIO_FLAG_READ_WRITE);
        // assert(ret >= 0);

        ret = avformat_write_header(fmt_ctx_, nullptr);
        assert(ret >= 0);
    }

    int update_stream(uint8_t *buf, int len)
    {
        std::scoped_lock lock(mutex_);
        std::copy(buf, buf + len, std::back_inserter(stream_));
        stream_len_ = stream_.size();

#if SAVE_TS_FILE
        ts_file_.write((char *)buf, len);
        ts_file_.flush();
#endif
        return len;
    }

private:
    int index_{ -1 };
    options opts_;

#if SAVE_TS_FILE
    std::ofstream ts_file_;
#endif

    AVFormatContext *fmt_ctx_{ nullptr };
    AVCodecContext *av_ctx_ = nullptr;
    AVStream *video_stream_{ nullptr };
    unsigned char *avio_buf_{ nullptr };
    AVIOContext *avio_ctx_{ nullptr };
    size_t frame_count_{ 0 };

    std::mutex mutex_;
    std::vector<uint8_t> stream_;
    std::atomic<size_t> stream_len_{ 0 };
};
