#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "time.h"
#include <Windows.h>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include <libswscale/swscale.h>
}

#define DEFAULT_FPS 30
#define DEFAULT_BPS 4000000

void encodeFrame(AVCodecContext *codec_context, AVFrame *frame, AVPacket *packet, AVFormatContext *format_context, AVStream *stream)
{

    int ret;

    // Send the frame to the encoder
    ret = avcodec_send_frame(codec_context, frame);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending a frame for encoding\n");
        exit(1);
    }

    // Receive encoded packets from the encoder
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(codec_context, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return;
        }
        else if (ret < 0)
        {
            fprintf(stderr, "Error receiving packet from encoder\n");
            exit(1);
        }

        // Rescale output packet timestamps to output stream timebase
        av_packet_rescale_ts(packet, codec_context->time_base, stream->time_base);
        packet->stream_index = stream->index;

        // Write the encoded packet to the output format
        ret = av_interleaved_write_frame(format_context, packet);
        if (ret < 0)
        {
            fprintf(stderr, "Error writing encoded frame to output\n");
            exit(1);
        }

        av_packet_unref(packet);
    }
}

int main(int argc, char **argv)
{
    avdevice_register_all();
    av_log_set_level(AV_LOG_DEBUG);

    // Set up codec and video stream context
    const AVCodec *codec = avcodec_find_encoder_by_name("h264_mf");
    if (!codec)
    {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    AVCodecContext *codec_context = avcodec_alloc_context3(codec);
    if (!codec_context)
    {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    codec_context->width = GetSystemMetrics(SM_CXSCREEN);
    codec_context->height = GetSystemMetrics(SM_CYSCREEN);
    codec_context->time_base = AVRational{ 1, DEFAULT_FPS };
    codec_context->framerate = AVRational{ DEFAULT_FPS, 1 };
    codec_context->bit_rate = DEFAULT_BPS;
    codec_context->gop_size = DEFAULT_FPS;
    codec_context->max_b_frames = 0;
    codec_context->pix_fmt = AV_PIX_FMT_YUV420P;

    if (codec->id == AV_CODEC_ID_H264)
    {
        av_opt_set(codec_context->priv_data, "preset", "ultrafast", 0);
        av_opt_set(codec_context->priv_data, "profile", "baseline", 0);
    }

    if (avcodec_open2(codec_context, codec, NULL) < 0)
    {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }

    AVFormatContext *format_context = nullptr;
    avformat_alloc_output_context2(&format_context, nullptr, "mpegts", "output.ts");
    auto output_format = format_context->oformat;

    // if (!(output_format->flags & AVFMT_NOFILE))
    //{
    //    if (avio_open(&format_context->pb, format_context->filename, AVIO_FLAG_WRITE) < 0)
    //    {
    //        fprintf(stderr, "Could not open output file\n");
    //        exit(1);
    //    }
    //}

    // Set up video stream
    AVStream *video_stream = avformat_new_stream(format_context, codec);
    if (!video_stream)
    {
        fprintf(stderr, "Could not allocate video stream\n");
        exit(1);
    }

    video_stream->id = format_context->nb_streams - 1;

    if (avcodec_parameters_from_context(video_stream->codecpar, codec_context) < 0)
    {
        fprintf(stderr, "Could not copy codec parameters to stream\n");
        exit(1);
    }

    video_stream->time_base = codec_context->time_base;

    if (avformat_write_header(format_context, NULL) < 0)
    {
        fprintf(stderr, "Could not write format header\n");
        exit(1);
    }

    // Set up screen capture
    AVFrame *frame = av_frame_alloc();
    if (!frame)
    {
        fprintf(stderr, "Could not allocate frame\n");
        exit(1);
    }

    int buffer_size = av_image_get_buffer_size(codec_context->pix_fmt, codec_context->width, codec_context->height, 1);
    uint8_t *buffer = (uint8_t *)av_malloc(buffer_size);
    if (!buffer)
    {
        fprintf(stderr, "Could not allocate image buffer\n");
        exit(1);
    }

    av_image_fill_arrays(frame->data, frame->linesize, buffer, codec_context->pix_fmt, codec_context->width, codec_context->height, 1);

    AVDictionary *options = NULL;
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options, "draw_mouse", "1", 0);

    AVInputFormat *input_format = av_find_input_format("gdigrab");

    AVFormatContext *input_format_context = NULL;
    if (avformat_open_input(&input_format_context, "desktop", input_format, &options) != 0)
    {
        fprintf(stderr, "Could not open input format\n");
        exit(1);
    }

    if (avformat_find_stream_info(input_format_context, NULL) < 0)
    {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }

    int video_stream_index = -1;
    for (int i = 0; i < input_format_context->nb_streams; i++)
    {
        AVStream *stream = input_format_context->streams[i];

        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_index = i;
            break;
        }
    }

    // Start capturing and encoding
    AVPacket packet = { 0 };
    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;

    int framesDecoded = 0;
    while (av_read_frame(format_context, &packet) == 0)
    {
        if (packet.stream_index == video_stream_index)
        {
            AVFrame *rawFrame = av_frame_alloc();
            if (!rawFrame)
            {
                fprintf(stderr, "Could not allocate raw frame\n");
                return 1;
            }

            int ret = avcodec_send_packet(codec_context, &packet);
            if (ret < 0)
            {
                fprintf(stderr, "Error sending packet for decoding\n");
                break;
            }

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(codec_context, rawFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0)
                {
                    fprintf(stderr, "Error receiving frame from decoder\n");
                    return 1;
                }

                sws_scale(sws_getCachedContext(NULL, codec_context->width, codec_context->height, AV_PIX_FMT_BGR0, codec_context->width, codec_context->height,
                              codec_context->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL),
                    rawFrame->data, rawFrame->linesize, 0, codec_context->height, frame->data, frame->linesize);

                frame->pts = framesDecoded++;

                // Encode the frame
                encodeFrame(codec_context, frame, &packet, format_context, video_stream);
            }

            av_frame_free(&rawFrame);
        }

        av_packet_unref(&packet);
    }

    // Clean up
    av_write_trailer(format_context);
    avcodec_close(codec_context);
    avcodec_free_context(&codec_context);
    av_frame_free(&frame);
    avformat_close_input(&input_format_context);
    avformat_free_context(format_context);

    return 0;
}
