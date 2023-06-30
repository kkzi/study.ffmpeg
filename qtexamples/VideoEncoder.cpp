#include "VideoEncoder.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>
}

struct VideoEncoder::Impl
{
    AVCodecContext* context_{ nullptr };
    //AVFrame* frame_{ nullptr };
    AVPacket* packet_{ nullptr };
    double frame_count_{ 0 };
    std::function<void(AVPacket*)> callback_;

    Impl() = default;
    ~Impl()
    {
        //av_frame_free(&frame_);
        av_packet_free(&packet_);
        avcodec_free_context(&context_);
    }

    static std::tuple<bool, std::string> init(VideoEncoder::Impl* impl, VideoEncoder::Options opts, AVFrame* frame)
    {
        //auto codec = avcodec_find_encoder(AV_CODEC_ID_H264)        
        //auto codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
        auto codec = avcodec_find_encoder_by_name(opts.codec_name.c_str());
        if (!codec) {
            return { false, std::string("Codec not found: ") + opts.codec_name };
        }

        auto ctx = avcodec_alloc_context3(codec);
        if (!ctx) {
            return { false ,"Could not allocate video codec context" };
        }
        impl->context_ = ctx;

        if (frame) {
            ctx->color_range = frame->color_range;
            ctx->color_primaries = frame->color_primaries;
            ctx->color_trc = frame->color_trc;
            ctx->colorspace = frame->colorspace;
            ctx->chroma_sample_location = frame->chroma_location;
        }

        ctx->bit_rate = opts.bitrate;
        ctx->width = opts.width;
        ctx->height = opts.height;
        ctx->coded_width = 0;
        ctx->coded_height = 0;
        ctx->time_base = AVRational{ 1, opts.framerate };
        ctx->framerate = AVRational{ opts.framerate, 1 };
        ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        ctx->codec_type = AVMEDIA_TYPE_VIDEO;
        ctx->field_order = AV_FIELD_PROGRESSIVE;

        //if (codec->id == AV_CODEC_ID_H264) {
        //    av_opt_set(ctx->priv_data, "preset", "slow", 0);
        //}

        //auto frame = av_frame_alloc();
        //if (!frame) {
        //    return { false, "Could not allocate video frame" };
        //}
        //impl->frame_ = frame;
        //frame->format = ctx->pix_fmt;
        //frame->width = ctx->width;
        //frame->height = ctx->height;


        auto pkt = av_packet_alloc();
        if (!pkt) {
            return { false, "alloc packet failed" };
        }

        AVDictionary* encoder_opts = NULL;
        if (!av_dict_get(encoder_opts, "threads", NULL, 0)) {
            av_dict_set(&encoder_opts, "threads", "auto", 0);
        }

        auto ret = avcodec_open2(ctx, codec, &encoder_opts);
        if (ret < 0) {
            char arr[64]{ 0 };
            auto err = av_make_error_string(arr, 64, ret);
            return { false, std::string("Could not open codec: ") + err };
        }

        return { true, "" };
    }
};


VideoEncoder::VideoEncoder(std::function<void(AVPacket*)> callback, Options opts)
    : callback_(std::move(callback))
    , opts_(std::move(opts))
{

}

VideoEncoder::~VideoEncoder()
{

}

void VideoEncoder::encode(AVFrame* frame)
{
    if (frame == nullptr) {
        return;
    }

    if (impl_ == nullptr) {
        init(frame);
    }

    auto ret = avcodec_send_frame(impl_->context_, frame);
    if (ret < 0 && !(ret == AVERROR_EOF && !frame)) {
        return;
    }

    while (true) {
        ret = avcodec_receive_packet(impl_->context_, impl_->packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return;
        }
        else if (ret < 0) {
            return;
        }

        //av_packet_rescale_ts(pkt, enc->time_base, ost->mux_timebase);

        impl_->callback_(impl_->packet_);
        av_packet_unref(impl_->packet_);
    }
}

void VideoEncoder::init(AVFrame* frame)
{
    impl_ = std::make_shared<Impl>();
    auto result = Impl::init(impl_.get(), opts_, frame);
    if (std::get<0>(result))
    {
        impl_->callback_ = callback_;
    }
    else
    {
        impl_.reset();
    }
}

