extern "C"
{
#include <libavformat/avformat.h>
}
#include <thread>

int run_server()
{
    avformat_network_init();

    AVFormatContext *fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx)
    {
        printf("Error allocating format context\n");
        return -1;
    }

    AVIOContext *io_ctx = NULL;
    if (avio_open2(&io_ctx, "tcp://127.0.0.1:1234", AVIO_FLAG_READ_WRITE, NULL, NULL) < 0)
    {
        printf("Error opening TCP connection\n");
        return -1;
    }

    fmt_ctx->pb = io_ctx;

    if (avformat_open_input(&fmt_ctx, NULL, NULL, NULL) != 0)
    {
        printf("Error opening input\n");
        return -1;
    }

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
    {
        printf("Error finding stream information\n");
        return -1;
    }

    av_dump_format(fmt_ctx, 0, "tcp://127.0.0.1:1234", 0);

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = (uint8_t *)"Hello All";
    pkt.size = strlen("Hello All");

    if (av_interleaved_write_frame(fmt_ctx, &pkt) < 0)
    {
        printf("Error writing frame\n");
        return -1;
    }

    av_write_trailer(fmt_ctx);

    avio_close(fmt_ctx->pb);
    avformat_free_context(fmt_ctx);
    avformat_network_deinit();
}

int run_client()
{
    avformat_network_init();

    AVFormatContext *fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx)
    {
        printf("Error allocating format context\n");
        return -1;
    }

    AVIOContext *io_ctx = NULL;
    if (avio_open2(&io_ctx, "tcp://127.0.0.1:1234", AVIO_FLAG_WRITE, NULL, NULL) < 0)
    {
        printf("Error opening TCP connection\n");
        return -1;
    }

    fmt_ctx->pb = io_ctx;

    AVStream *out_stream = avformat_new_stream(fmt_ctx, NULL);
    if (!out_stream)
    {
        printf("Error creating new stream\n");
        return -1;
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = (uint8_t *)"Hello World";
    pkt.size = strlen("Hello World");

    if (av_write_frame(fmt_ctx, &pkt) < 0)
    {
        printf("Error writing frame\n");
        return -1;
    }

    av_write_trailer(fmt_ctx);

    avio_close(fmt_ctx->pb);
    avformat_free_context(fmt_ctx);
    avformat_network_deinit();
}

int main(int argc, char **argv)
{
    std::thread t(run_server);
    run_client();
    t.join();
}
