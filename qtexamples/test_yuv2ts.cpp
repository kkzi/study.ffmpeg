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

    int in_width = 960;              // ������ͼ����
    int in_height = 544;             // �����ͼ��߶�
    int out_width = in_width / 2;    //�����Ƶͼ����
    int out_height = in_height / 2;  //�����Ƶͼ��߶�
    int frame_rate = 20;             //�����Ƶ֡��
    int bitrate = 3000000;           //�����Ƶ������

    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    AVStream *video_st = NULL;
    AVCodecContext *pCodecCtx = NULL;
    AVCodec *pCodec = NULL;
    struct SwsContext *sws_ctx = NULL;

    AVFrame *picture = NULL;          // ����ͼ��ṹ��
    uint8_t *picture_buf = NULL;      // yuv420 data������ͼ��buffer
    AVFrame *out_picture = NULL;      // ���ͼ��ṹ�壬��Ϊ������������
    uint8_t *out_picture_buf = NULL;  //���ͼ��buffer
    int picture_size = 0;             // ����ͼ�����ݴ�С��yuv420��ʽ�£�size = width*height*3/2
    int out_picture_size = 0;         // ���ͼ�����ݴ�С�����ʽ�й�

    AVPacket enc_pkt;  // �������ݰ�
    int64_t frame_pts = 0;
    int ret;
    size_t read_size;
    bool isrgb = false;  //ָʾ�����Ƿ���RGB�����ݣ�trueΪRGB���ݣ�falseΪYUV����

    /* ������YUV��ʽ�ļ� */
    FILE *in_file = fopen(in_filename, "rb+");
    if (!in_file)
    {
        cout << "Open input file failed! " << endl;
        return -1;
    }

    /* һϵ�г�ʼ������ */
    avcodec_register_all();
    av_register_all();
    // avformat_network_init();   //������Ҫ�ù���

    /* ������������� */
    avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpegts", out_filename);
    if (!ofmt_ctx)
    {
        cout << "Could not create output context!" << endl;
        goto end;
    }

    // �����ʽ
    ofmt = ofmt_ctx->oformat;

    /* ������ļ� */
    if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_READ_WRITE) < 0)
    {
        cout << "Open output file failed!" << endl;
        goto end;
    }

    //Ϊ����ļ�����һ���µ�stream
    video_st = avformat_new_stream(ofmt_ctx, NULL);
    if (!video_st)
    {
        cout << "Allocate output stream failed" << endl;
        goto end;
    }

    // Set frame rate
    video_st->time_base.num = 1;
    video_st->time_base.den = frame_rate;

    /* ���ñ������ */
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

    /* ���ݱ�����ID����ע����ı����� */
    pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
    if (!pCodec)
    {
        cout << "Can not find encoder!" << endl;
        goto end;
    }

    /* ������֪��AVCodec��ʼ��AVCodecContext */
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
    {
        cout << "Open encoder failed!" << endl;
        goto end;
    }

    // ��ӡ�����Ϣ
    av_dump_format(ofmt_ctx, 0, out_filename, 1);

    /* ��������ͼ���ʽ */
    picture = av_frame_alloc();
    picture->width = in_width;
    picture->height = in_height;
    picture->format = AV_PIX_FMT_YUV420P;
    picture_size = avpicture_get_size((enum AVPixelFormat)picture->format, picture->width, picture->height);
    picture_buf = (uint8_t *)av_malloc(picture_size);
    avpicture_fill((AVPicture *)picture, (const uint8_t *)picture_buf, AV_PIX_FMT_YUV420P, in_width, in_height);

    /* �������ͼ��(�����������ͼ��)��ʽ */
    out_picture = av_frame_alloc();
    out_picture->width = pCodecCtx->width;
    out_picture->height = pCodecCtx->height;
    out_picture->format = pCodecCtx->pix_fmt;
    out_picture_size = avpicture_get_size((enum AVPixelFormat)out_picture->format, out_picture->width, out_picture->height);
    out_picture_buf = (uint8_t *)av_malloc(out_picture_size);
    avpicture_fill((AVPicture *)out_picture, (const uint8_t *)out_picture_buf, AV_PIX_FMT_YUV420P, out_width, out_height);

    /* ͼ��rescale������ת��ͼ���ʽ��������ΪRGB��ʽ���ù��ܺ���Ҫ */
    sws_ctx = sws_getContext(in_width, in_height, AV_PIX_FMT_YUV420P, out_width, out_height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx)
    {
        cout << "Impossible to create scale context for format conversion!" << endl;
        return -1;
    }

    /* д����ļ�ͷ */
    avformat_write_header(ofmt_ctx, NULL);

    av_new_packet(&enc_pkt, out_picture_size);

    while (1)
    {
        read_size = fread(picture_buf, 1, picture_size, in_file);

        if (feof(in_file))
        {
            break;
        }

        /* ����ȡ����buffer������䵽picture�ṹ���� */
        avpicture_fill((AVPicture *)picture, (const uint8_t *)picture_buf, AV_PIX_FMT_YUV420P, in_width, in_height);

        /* ���ݱ������Ҫ�󣬽�����ͼ���ʽת���ɱ�������Ҫ��ͼ���ʽ */
        sws_scale(sws_ctx, picture->data, picture->linesize, 0, in_height, out_picture->data, out_picture->linesize);

        /* ���õ�ǰ֡��ʱ��� */
        out_picture->pts = frame_pts++;
        int got_pkt = 0;

        // ����һ֡����
        ret = avcodec_encode_video2(pCodecCtx, &enc_pkt, out_picture, &got_pkt);
        if (ret < 0)
        {
            cout << "Encode failed!" << endl;
            return -1;
        }

        /* ��������һ�����ݴ���ʱ�����Ȼ��д������ļ��� */
        if (got_pkt)
        {
            enc_pkt.stream_index = video_st->index;
            av_packet_rescale_ts(&enc_pkt, pCodecCtx->time_base, video_st->time_base);  //����timebase�����������ʱ���
            enc_pkt.pos = -1;
            ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);  // Write the encoded packet to the output file
            av_free_packet(&enc_pkt);
        }
    }

    /* �������ʱ�����������л����ͼ���������������ļ� */
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
