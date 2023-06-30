#include "VideoOutput.h"
#include "ffmpeg.hpp"
#include <cassert>

struct output_stream
{
    AVStream* st{ nullptr };
    AVCodecContext* enc{ nullptr };
    const AVCodec* codec{ nullptr };
    int64_t next_pts{ 0 };
    int samples_count{ 0 };
    AVFrame* frame{ nullptr };
    AVPacket* tmp_pkt{ nullptr };
    float t{ 0 };
    float tincr{ 0 };
    float tincr2{ 0 };
    struct SwsContext* sws_ctx{ nullptr };
    struct SwrContext* swr_ctx{ nullptr };
};

static AVFrame* alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
    auto frame = av_frame_alloc();
    if (frame == nullptr) {
        throw std::runtime_error("Could not allocate frame.");
    }
    frame->format = pix_fmt;
    frame->width = width;
    frame->height = height;

    /* allocate the buffers for the frame data */
    if (av_frame_get_buffer(frame, 0) < 0) {
        throw std::runtime_error("Could not allocate frame data.");
    }
    return frame;
}


/* Add an output stream. */
static void open_stream(output_stream* ost, AVFormatContext* oc, enum AVCodecID codec_id, int width, int height, int framerate)
{
#define STREAM_DURATION   10.0
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

    /* find the encoder */
    ost->codec = avcodec_find_encoder(codec_id);
    if (ost->codec == nullptr) {
        throw std::runtime_error(std::format("Could not find encoder for {}", avcodec_get_name(codec_id)));
    }

    ost->tmp_pkt = av_packet_alloc();
    if (!ost->tmp_pkt) {
        std::runtime_error("Could not allocate AVPacket");
    }

    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {
        std::runtime_error("Could not allocate stream");
    }
    ost->st->id = oc->nb_streams - 1;
    auto c = avcodec_alloc_context3(ost->codec);
    if (!c) {
        std::runtime_error("Could not alloc an encoding context");
    }
    ost->enc = c;

    switch ((ost->codec)->type) {
    case AVMEDIA_TYPE_AUDIO:
        c->sample_fmt = (ost->codec)->sample_fmts ?
            (ost->codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        c->bit_rate = 64000;
        c->sample_rate = 44100;
        if ((ost->codec)->supported_samplerates) {
            c->sample_rate = (ost->codec)->supported_samplerates[0];
            for (auto i = 0; (ost->codec)->supported_samplerates[i]; i++) {
                if ((ost->codec)->supported_samplerates[i] == 44100)
                    c->sample_rate = 44100;
            }
        }
        {
            AVChannelLayout layout = AV_CHANNEL_LAYOUT_STEREO;
            av_channel_layout_copy(&c->ch_layout, &layout);
        }
        ost->st->time_base = AVRational{ 1, c->sample_rate };
        break;

    case AVMEDIA_TYPE_VIDEO:
        c->codec_id = codec_id;
        c->bit_rate = 400000;
        c->width = width;
        c->height = height;
        ost->st->time_base = AVRational{ 1, framerate };
        c->time_base = ost->st->time_base;
        c->gop_size = 12; /* emit one intra frame every twelve frames at most */
        c->pix_fmt = STREAM_PIX_FMT;

        break;
    default:
        break;
    }

    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

static void open_video(AVFormatContext* oc, output_stream* ost, AVDictionary* opt_arg)
{
    AVDictionary* opt = NULL;
    av_dict_copy(&opt, opt_arg, 0);

    AVCodecContext* c = ost->enc;
    auto ret = avcodec_open2(c, ost->codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        throw std::runtime_error(std::format("Could not open video codec: {}", ff_err2str(ret)));
    }

    /* allocate and init a re-usable frame */
    ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);


    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        std::runtime_error("Could not copy the stream parameters");
    }
}

static int write_frame(AVFormatContext* fmctx, AVCodecContext* avctx, AVStream* st, AVFrame* frame, AVPacket* pkt)
{
    auto ret = avcodec_send_frame(avctx, frame);
    if (ret < 0) {
        throw std::runtime_error(std::format("Error sending a frame to the encoder: {}", ff_err2str(ret)));
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(avctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0) {
            throw std::runtime_error(std::format("Error encoding a frame: {}", ff_err2str(ret)));
        }

        /* rescale output packet timestamp values from codec to stream timebase */
        av_packet_rescale_ts(pkt, avctx->time_base, st->time_base);
        pkt->stream_index = st->index;

        ret = av_interleaved_write_frame(fmctx, pkt);
        /* pkt is now blank (av_interleaved_write_frame() takes ownership of
         * its contents and resets pkt), so that no unreferencing is necessary.
         * This would be different if one used av_write_frame(). */
        if (ret < 0) {
            throw std::runtime_error(std::format("Error while writing output packet: {}", ff_err2str(ret)));
        }
    }

    return ret == AVERROR_EOF ? 1 : 0;
}



VideoOutput::VideoOutput() = default;

VideoOutput::~VideoOutput()
{
    if (!(fmtctx->flags & AVFMT_NOFILE)) {
        avio_closep(&fmtctx->pb);
    }

    avformat_free_context(fmtctx);
}

void VideoOutput::prepare(std::string_view filename, std::string_view codec, int width, int height, int framerate)
{
    /* allocate the output media context */
    avformat_alloc_output_context2(&fmtctx, NULL, codec.data(), filename.data());
    if (!fmtctx) {
        throw std::runtime_error(std::format("Could not deduce output format from filename: {}", filename));
    }

    outfmt = fmtctx->oformat;
    if (outfmt->video_codec != AV_CODEC_ID_NONE) {
        auto vs = new output_stream;
        open_stream(vs, fmtctx, outfmt->video_codec, width, height, framerate);
        video = std::unique_ptr<output_stream>(vs);
    }
    if (video != nullptr) {
        open_video(fmtctx, video.get(), opt);
        encode_video = true;
    }

    if (!(outfmt->flags & AVFMT_NOFILE)) {
        auto ret = avio_open(&fmtctx->pb, filename.data(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            throw std::runtime_error(std::format("Could not open {}: {}", filename, ff_err2str(ret)));
        }
    }

    auto ret = avformat_write_header(fmtctx, &opt);
    if (ret < 0) {
        throw std::runtime_error(std::format("Error occurred when opening output file: {}", ff_err2str(ret)));
    }

    //while (encode_video || encode_audio) {
    //    /* select the stream to encode */
    //    if (encode_video && (!encode_audio || av_compare_ts(video->next_pts, video->enc->time_base, audio->next_pts, audio->enc->time_base) <= 0)) {
    //        encode_video = !write_video_frame(fmtctx, video.get());
    //    }
    //    else {
    //        //encode_audio = !write_audio_frame(fmtctx, *audio);
    //    }
    //}
}

void VideoOutput::write(AVFrame * frame)
{
    assert(video != nullptr);
    assert(frame != nullptr);
    if (video == nullptr)
    {
        return;
    }


    AVCodecContext* c = video->enc;

    //video->frame = av_frame_clone(frame);

    /* check if we want to generate more frames */
    //if (av_compare_ts(video->next_pts, c->time_base, STREAM_DURATION, AVRational{ 1, 1 }) > 0)
    //{
    //    return;
    //}

    //if (av_frame_make_writable(video->frame) < 0)
    //{
    //    return;
    //}

    video->sws_ctx = sws_getCachedContext(video->sws_ctx, frame->width, frame->height,
        AV_PIX_FMT_YUV420P,
        frame->width, frame->height,
        c->pix_fmt,
        SWS_BICUBIC, NULL, NULL, NULL);
    sws_scale(video->sws_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0, frame->height, video->frame->data, video->frame->linesize);

    //c->width = frame->width;
    //c->height = frame->height;

    //video->frame = frame;
    //av_frame_ref(video->frame, frame);
    video->frame->pts = video->next_pts++;
    write_frame(fmtctx, video->enc, video->st, video->frame, video->tmp_pkt);
    //av_frame_unref(frame);
    //av_frame_free(&video->frame);
}

void VideoOutput::finish()
{
    av_write_trailer(fmtctx);
}
