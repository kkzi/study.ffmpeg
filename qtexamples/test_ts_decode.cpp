#include "ffmpeg.hpp"
#include "ts_decode.hpp"
#include <QImage>
#include <fstream>
#include <thread>

static int i = 0;
int main(int argc, char **argv)
{
    ts_decode decode(0, {});
    decode.start([](auto &&buf, auto &&len) {
        // auto pixmap = ff_from_yuv();
        QImage image((uchar *)buf, 400, 200, QImage::Format_RGB32);
        image.save(QString("www_recv_%1.jpg").arg(i++));
    });

    // std::ifstream file("www_pcm_0.ts", std::ios::binary);
    std::ifstream file("www_recv_0.ts", std::ios::binary);
    while (file.good())
    {
        std::vector<uint8_t> frame(0xfc);
        file.read((char *)frame.data(), frame.size());
        decode.push_bytes(frame);
    }
}
