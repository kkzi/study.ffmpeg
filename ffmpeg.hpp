#pragma once

#include <cassert>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

extern "C"
{
#include "libswscale/swscale.h"
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/imgutils.h>
}

using ff_frame_callback = std::function<void(AVFrame *)>;
using ff_packet_callback = std::function<void(AVPacket *)>;

static std::string ff_err2str(int ret)
{
    char arr[AV_ERROR_MAX_STRING_SIZE]{ 0 };
    return av_make_error_string(arr, AV_ERROR_MAX_STRING_SIZE, ret);
}

#define REQUIRE_PTR(ptr, err, ...)                                                                                                                             \
    if (ptr == nullptr) throw std::runtime_error(std::format(err, __VA_ARGS__));

#define REQUIRE_RET(ret)                                                                                                                                       \
    if (ret < 0) throw std::runtime_error(ff_err2str(ret));

static void ff_save_yuv_file(std::ofstream &out, AVFrame *frame)
{
    auto len = frame->width * frame->height;
    out.write((char *)frame->data[0], len);
    out.write((char *)frame->data[1], len / 4);
    out.write((char *)frame->data[2], len / 4);
    out.flush();
}

static AVFrame *ff_alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
    if (auto picture = av_frame_alloc(); picture != nullptr)
    {
        picture->format = pix_fmt;
        picture->width = width;
        picture->height = height;

        /* allocate the buffers for the frame data */
        auto ret = av_frame_get_buffer(picture, 1);
        if (ret >= 0)
        {
            return picture;
        }
        av_frame_free(&picture);
    }
    return nullptr;
}

static std::vector<AVFrame *> ff_decode(AVCodecContext *ctx, AVPacket *pkt)
{
    auto ret = avcodec_send_packet(ctx, pkt);
    if (ret < 0)
    {
        return {};
    }

    std::vector<AVFrame *> frames;
    while (true)
    {
        auto frame = av_frame_alloc();
        if (avcodec_receive_frame(ctx, frame) >= 0)
        {
            frames.push_back(frame);
        }
        else  // ret == AVERROR(EAGAIN) || ret == AVERROR_EOF
        {
            av_frame_free(&frame);
            break;
        }
    }
    return frames;
}

static std::vector<AVPacket *> ff_encode(AVCodecContext *ctx, AVFrame *frame)
{
    auto ret = avcodec_send_frame(ctx, frame);
    if (ret < 0)
    {
        return {};
    }
    std::vector<AVPacket *> packets;
    while (true)
    {
        auto pkt = av_packet_alloc();
        if (auto ret = avcodec_receive_packet(ctx, pkt); ret >= 0)
        {
            //if (frame != nullptr)
            //{
            //    pkt->time_base = frame->time_base;
            //    pkt->pts = frame->pts;
            //    pkt->dts = frame->pkt_dts;
            //}
            packets.push_back(pkt);
        }
        else  // ret == AVERROR(EAGAIN) || ret == AVERROR_EOF
        {
            auto err = ff_err2str(ret);
            av_packet_free(&pkt);
            break;
        }
    }
    return packets;
}

static std::vector<uint8_t> ff_from_yuv(AVFrame *frame, AVPixelFormat to_fmt, SwsContext *swsctx)
{
    assert(frame != nullptr);

    static AVPixelFormat from_fmt = AV_PIX_FMT_YUV420P;
    if (from_fmt == to_fmt)
    {
        return std::vector<uint8_t>(frame->data[0], frame->data[0] + frame->linesize[0]);
    }
    else
    {
        auto width = frame->width;
        auto height = frame->height;

        uint8_t *pixels[4]{ 0 };
        int pitch[4]{ 0 };
        av_image_alloc(pixels, pitch, width, height, to_fmt, 1);
        sws_scale(swsctx, frame->data, frame->linesize, 0, height, pixels, pitch);
        std::vector<uint8_t> pixmap(pitch[0]);
        std::copy(pixels[0], pixels[0] + pitch[0], pixmap.begin());
        av_freep(&pixels[0]);
        return pixmap;
    }
}
