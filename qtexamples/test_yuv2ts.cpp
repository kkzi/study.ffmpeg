#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libavutil/avutil.h"
    #include "libswscale/swscale.h"
    #include <libavutil/opt.h>
}
#endif

using namespace std;

static int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index)
{
    int ret;
    int got_frame;
    AVPacket enc_pkt;

    if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities & AV_CODEC_CAP_DELAY))
        return 0;

    while (1)

    {
        enc_pkt.data = NULL;
        enc_pkt.size = 0;
        av_init_packet(&enc_pkt);
        ret = avcodec_encode_video2(fmt_ctx->streams[stream_index]->codec, &enc_pkt, NULL, &got_frame);

        av_frame_free(NULL);

        if (ret < 0)
        {
            break;
        }
        if (!got_frame)
        {
            ret = 0;
            break;
        }

        enc_pkt.stream_index = stream_index;
        av_packet_rescale_ts(&enc_pkt, fmt_ctx->streams[stream_index]->codec->time_base, fmt_ctx->streams[stream_index]->time_base);

        ret = av_interleaved_write_frame(fmt_ctx, &enc_pkt);
        if (ret < 0)
        {
            break;
        }
    }

    return ret;
}

int main(int argc, char *argv[])
{
    const char *in_filename = "littlegirl.yuv";
    // const char *out_filename = "udp://233.233.233.223:5555";
    const char *out_filename = "littlegirl.ts";

    int in_width = 960;              // 输入裸图像宽度
    int in_height = 544;             // 输出裸图像高度
    int out_width = in_width / 2;    //输出视频图像宽度
    int out_height = in_height / 2;  //输出视频图像高度
    int frame_rate = 20;             //输出视频帧率
    int bitrate = 3000000;           //输出视频比特率

    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    AVStream *video_st = NULL;
    AVCodecContext *pCodecCtx = NULL;
    AVCodec *pCodec = NULL;
    struct SwsContext *sws_ctx = NULL;

    AVFrame *picture = NULL;          // 输入图像结构体
    uint8_t *picture_buf = NULL;      // yuv420 data，输入图像buffer
    AVFrame *out_picture = NULL;      // 输出图像结构体，作为编码器的输入
    uint8_t *out_picture_buf = NULL;  //输出图像buffer
    int picture_size = 0;             // 输入图像数据大小，yuv420格式下，size = width*height*3/2
    int out_picture_size = 0;         // 输出图像数据大小，与格式有关

    AVPacket enc_pkt;  // 编码数据包
    int64_t frame_pts = 0;
    int ret;
    size_t read_size;
    bool isrgb = false;  //指示输入是否是RGB裸数据，true为RGB数据，false为YUV数据

    /* 打开输入YUV格式文件 */
    FILE *in_file = fopen(in_filename, "rb+");
    if (!in_file)
    {
        cout << "Open input file failed! " << endl;
        return -1;
    }

    /* 一系列初始化操作 */
    avcodec_register_all();
    av_register_all();
    // avformat_network_init();   //推流需要该功能

    /* 分配输出上下文 */
    avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpegts", out_filename);
    if (!ofmt_ctx)
    {
        cout << "Could not create output context!" << endl;
        goto end;
    }

    // 输出格式
    ofmt = ofmt_ctx->oformat;

    /* 打开输出文件 */
    if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_READ_WRITE) < 0)
    {
        cout << "Open output file failed!" << endl;
        goto end;
    }

    //为输出文件创建一个新的stream
    video_st = avformat_new_stream(ofmt_ctx, NULL);
    if (!video_st)
    {
        cout << "Allocate output stream failed" << endl;
        goto end;
    }

    // Set frame rate
    video_st->time_base.num = 1;
    video_st->time_base.den = frame_rate;

    /* 设置编码参数 */
    pCodecCtx = video_st->codec;
    pCodecCtx->codec_id = AV_CODEC_ID_H264;
    pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    pCodecCtx->width = out_width;
    pCodecCtx->height = out_height;

    pCodecCtx->time_base.num = 1;
    pCodecCtx->time_base.den = frame_rate;
    pCodecCtx->bit_rate = bitrate;
    pCodecCtx->gop_size = frame_rate;  // 1 second 1 gop

    if (pCodecCtx->codec_id == AV_CODEC_ID_H264)
    {
        pCodecCtx->qmin = 10;
        pCodecCtx->qmax = 51;
        pCodecCtx->qcompress = 0.6;

        pCodecCtx->thread_count = 6;
        pCodecCtx->thread_type = FF_THREAD_FRAME;
        pCodecCtx->profile = FF_PROFILE_H264_BASELINE;
        av_opt_set(pCodecCtx->priv_data, "preset", "superfast", 0);
        av_opt_set(pCodecCtx->priv_data, "tune", "zerolatency", 0);
    }

    /* 根据编码器ID查找注册过的编码器 */
    pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
    if (!pCodec)
    {
        cout << "Can not find encoder!" << endl;
        goto end;
    }

    /* 利用已知的AVCodec初始化AVCodecContext */
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
    {
        cout << "Open encoder failed!" << endl;
        goto end;
    }

    // 打印输出信息
    av_dump_format(ofmt_ctx, 0, out_filename, 1);

    /* 设置输入图像格式 */
    picture = av_frame_alloc();
    picture->width = in_width;
    picture->height = in_height;
    picture->format = AV_PIX_FMT_YUV420P;
    picture_size = avpicture_get_size((enum AVPixelFormat)picture->format, picture->width, picture->height);
    picture_buf = (uint8_t *)av_malloc(picture_size);
    avpicture_fill((AVPicture *)picture, (const uint8_t *)picture_buf, AV_PIX_FMT_YUV420P, in_width, in_height);

    /* 设置输出图像(送入编码器的图像)格式 */
    out_picture = av_frame_alloc();
    out_picture->width = pCodecCtx->width;
    out_picture->height = pCodecCtx->height;
    out_picture->format = pCodecCtx->pix_fmt;
    out_picture_size = avpicture_get_size((enum AVPixelFormat)out_picture->format, out_picture->width, out_picture->height);
    out_picture_buf = (uint8_t *)av_malloc(out_picture_size);
    avpicture_fill((AVPicture *)out_picture, (const uint8_t *)out_picture_buf, AV_PIX_FMT_YUV420P, out_width, out_height);

    /* 图像rescale，包括转换图像格式，若输入为RGB格式，该功能很重要 */
    sws_ctx = sws_getContext(in_width, in_height, AV_PIX_FMT_YUV420P, out_width, out_height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx)
    {
        cout << "Impossible to create scale context for format conversion!" << endl;
        return -1;
    }

    /* 写输出文件头 */
    avformat_write_header(ofmt_ctx, NULL);

    av_new_packet(&enc_pkt, out_picture_size);

    while (1)
    {
        read_size = fread(picture_buf, 1, picture_size, in_file);

        if (feof(in_file))
        {
            break;
        }

        /* 将读取到的buffer数据填充到picture结构体中 */
        avpicture_fill((AVPicture *)picture, (const uint8_t *)picture_buf, AV_PIX_FMT_YUV420P, in_width, in_height);

        /* 根据编码输出要求，将输入图像格式转换成编码器需要的图像格式 */
        sws_scale(sws_ctx, picture->data, picture->linesize, 0, in_height, out_picture->data, out_picture->linesize);

        /* 设置当前帧的时间戳 */
        out_picture->pts = frame_pts++;
        int got_pkt = 0;

        // 编码一帧数据
        ret = avcodec_encode_video2(pCodecCtx, &enc_pkt, out_picture, &got_pkt);
        if (ret < 0)
        {
            cout << "Encode failed!" << endl;
            return -1;
        }

        /* 将编码后的一包数据打上时间戳，然后写到输出文件中 */
        if (got_pkt)
        {
            enc_pkt.stream_index = video_st->index;
            av_packet_rescale_ts(&enc_pkt, pCodecCtx->time_base, video_st->time_base);  //根据timebase计算输出流的时间戳
            enc_pkt.pos = -1;
            ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);  // Write the encoded packet to the output file
            av_free_packet(&enc_pkt);
        }
    }

    /* 编码结束时，将编码器中缓存的图像数据输出到输出文件 */
    ret = flush_encoder(ofmt_ctx, 0);
    if (ret < 0)
    {
        cout << "Flushing encoder failed." << endl;
        return -1;
    }

    // Write the output file trailer
    av_write_trailer(ofmt_ctx);

end:
    if (ofmt_ctx)
    {
        avio_close(ofmt_ctx->pb);
        avformat_free_context(ofmt_ctx);
    }

    ret = fclose(in_file);
    if (!ret)
    {
        cout << "Input file closed OK. " << endl;
    }
    else
    {
        cout << "Input file closed failed." << endl;
    }

    if (video_st)
    {
        avcodec_close(video_st->codec);
        video_st = NULL;
    }

    if (picture)
    {
        av_frame_free(&picture);
        picture = NULL;
    }

    if (picture_buf)
    {
        av_freep(&picture_buf);
    }

    if (out_picture_buf)
    {
        av_freep(&out_picture_buf);
    }

    if (sws_ctx)
    {
        sws_freeContext(sws_ctx);
    }
}
