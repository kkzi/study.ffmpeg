
/**
 * ��򵥵Ļ���FFmpeg��AVDevice���ӣ���Ļ¼�ƣ�
 * Simplest FFmpeg Device (Screen Capture)
 *
 * �����ף����Ǿ���Ϊ��ʵ��������Ļ¼�Ƶ����
 * ffmpeg -f gdigrab -i desktop out.mpg
 *
 * ������ʵ������Ļ¼�ƹ��ܡ�����¼�Ʋ������������ݡ��ǻ���FFmpeg
 * ��libavdevice�����򵥵����ӡ�ͨ�������ӣ�����ѧϰFFmpeg��
 * libavdevice����ʹ�÷�����
 * ��������Windows�¿���ʹ��2�ַ�ʽ¼����Ļ��
 *  1.gdigrab: Win32�µĻ���GDI����Ļ¼���豸��
 *             ץȡ�����ʱ������URLΪ��desktop����
 *  2.dshow: ʹ��Directshow��ע����Ҫ��װ��������screen-capture-recorder
 * ��Linux�¿���ʹ��x11grab¼����Ļ��
 * ��MacOS�¿���ʹ��avfoundation¼����Ļ��
 */
#include <stdio.h>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavutil/dict.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}

#include "ffmpeg.hpp"
#include <format>

#define OUTPUT_YUV420P 1
#define OUTPUT_H264 1

int main(int argc, char *argv[])
{
    AVFormatContext *pFormatCtx;
    AVStream *videoStream;
    AVCodecContext *pCodecCtx;
    const AVCodec *pCodec;
    AVFrame *pFrame, *pFrameYUV;
    AVPacket *pPacket;
    SwsContext *pImgConvertCtx;

    int videoIndex = -1;
    unsigned int i = 0;

    int screen_w = 0;
    int screen_h = 0;

    printf("Starting...\n");

    // register device
    avdevice_register_all();

    pFormatCtx = avformat_alloc_context();

    // use gdigrab
    auto ifmt = av_find_input_format("gdigrab");
    if (!ifmt)
    {
        printf("can't find input device.\n");
        return -1;
    }

    int framerate = 1;

    AVDictionary *options = NULL;
    av_dict_set_int(&options, "framerate", framerate, 0);
    av_dict_set_int(&options, "offset_x", 0, 0);
    av_dict_set_int(&options, "offset_y", 0, 0);
    av_dict_set(&options, "video_size", std::format("{}x{}", 400, 200).c_str(), 0);
    if (avformat_open_input(&pFormatCtx, "desktop", ifmt, &options) != 0)
    {
        printf("can't open input stream.\n");
        return -1;
    }

    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        printf("can't find stream information.\n");
        return -1;
    }

    for (i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoIndex = i;
            break;
        }
    }

    if (videoIndex == -1)
    {
        printf("can't find a video stream.\n");
        return -1;
    }

    videoStream = pFormatCtx->streams[videoIndex];
    pCodec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (pCodec == NULL)
    {
        printf("codec not found.\n");
        return -1;
    }

    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx)
    {
        printf("can't alloc codec context.\n");
        return -1;
    }

    avcodec_parameters_to_context(pCodecCtx, videoStream->codecpar);

    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
    {
        printf("can't open codec.\n");
        return -1;
    }

    pFrame = av_frame_alloc();
    // pFrameYUV = av_frame_alloc();
    pPacket = av_packet_alloc();

    pFrameYUV = ff_alloc_picture(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);
    // unsigned char *outBuffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
    // av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, outBuffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);
    pImgConvertCtx = sws_getContext(
        pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

#if OUTPUT_YUV420P
    FILE *fpYUV = fopen("output.yuv", "wb+");
#endif

#if OUTPUT_H264
    AVCodecContext *pH264CodecCtx;
    const AVCodec *pH264Codec;

    FILE *fpH264 = fopen("output.h264", "wb+");

    //����H264������
    pH264Codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!pH264Codec)
    {
        printf("can't find h264 codec.\n");
        return -1;
    }

    pH264CodecCtx = avcodec_alloc_context3(pH264Codec);
    pH264CodecCtx->codec_id = AV_CODEC_ID_H264;
    pH264CodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pH264CodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    pH264CodecCtx->width = pCodecCtx->width;
    pH264CodecCtx->height = pCodecCtx->height;
    pH264CodecCtx->time_base.num = 1;
    pH264CodecCtx->time_base.den = framerate;  //֡�ʣ���һ���Ӷ�����ͼƬ��
    pH264CodecCtx->bit_rate = 400000;          //�����ʣ����������С���Ըı�������Ƶ��������
    // pH264CodecCtx->gop_size = 12;
    pH264CodecCtx->gop_size = 3;
    pH264CodecCtx->qmin = 10;
    pH264CodecCtx->qmax = 51;
    // some formats want stream headers to be separate
    if (pH264CodecCtx->flags & AVFMT_GLOBALHEADER)
    {
        pH264CodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // set option
    AVDictionary *params = NULL;
    // H.264
    av_dict_set(&params, "preset", "superfast", 0);
    av_dict_set(&params, "tune", "zerolatency", 0);  //ʵ��ʵʱ����
    if (avcodec_open2(pH264CodecCtx, pH264Codec, &params) < 0)
    {
        printf("can't open video encoder.\n");
        return -1;
    }

#endif

    screen_w = pCodecCtx->width;
    screen_h = pCodecCtx->height;

    while (av_read_frame(pFormatCtx, pPacket) >= 0)
    {
        if (pPacket->stream_index == videoIndex)
        {
            int ret = avcodec_send_packet(pCodecCtx, pPacket);
            if (ret < 0)
            {
                printf("Decode error.\n");
                return -1;
            }

            if (avcodec_receive_frame(pCodecCtx, pFrame) >= 0)
            {
                sws_scale(
                    pImgConvertCtx, (const unsigned char *const *)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);

#if OUTPUT_YUV420P
                int y_size = pCodecCtx->width * pCodecCtx->height;
                fwrite(pFrameYUV->data[0], 1, y_size, fpYUV);      // Y
                fwrite(pFrameYUV->data[1], 1, y_size / 4, fpYUV);  // U
                fwrite(pFrameYUV->data[2], 1, y_size / 4, fpYUV);  // V
#endif

#if OUTPUT_H264
                pFrameYUV->format = AV_PIX_FMT_YUV420P;
                pFrameYUV->width = pCodecCtx->width;
                pFrameYUV->height = pCodecCtx->height;
                pFrameYUV->color_primaries = pCodecCtx->color_primaries;
                pFrameYUV->color_range = pCodecCtx->color_range;
                pFrameYUV->color_trc = pCodecCtx->color_trc;
                pFrameYUV->colorspace = pCodecCtx->colorspace;
                int ret = avcodec_send_frame(pH264CodecCtx, pFrameYUV);
                if (ret < 0)
                {
                    printf("failed to encode. %s\n", ff_err2str(ret).c_str());
                    return -1;
                }

                if (avcodec_receive_packet(pH264CodecCtx, pPacket) >= 0)
                {
                    ret = fwrite(pPacket->data, 1, pPacket->size, fpH264);
                    if (ret < 0)
                    {
                        printf("write into output.h264 failed.\n");
                    }
                }
#endif
            }
        }

        av_packet_unref(pPacket);
    }

    sws_freeContext(pImgConvertCtx);

#if OUTPUT_YUV420P
    fclose(fpYUV);
#endif

#if OUTPUT_H264
    fclose(fpH264);
#endif

    // av_free(outBuffer);
    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);
#if OUTPUT_H264
    avcodec_close(pH264CodecCtx);
#endif
    avformat_close_input(&pFormatCtx);

    return 0;
}
