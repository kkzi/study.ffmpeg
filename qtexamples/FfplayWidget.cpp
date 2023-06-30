#include "FfplayWidget.h"
#include "VideoDecoder.h"
#include <fstream>
#include <QThread>

FfplayWidget::FfplayWidget(QWidget* parent)
    : QLabel(parent)
{
    connect(this, &FfplayWidget::imageReceived, this, &FfplayWidget::paintImage);
}

void FfplayWidget::open(const QString& url)
{
    decode_ = std::make_shared<VideoDecoder>();
    std::ofstream file("test_3.rgb", std::ios::binary);
    if (auto [ok, info] = decode_->open(url.toStdString());  ok) {
        decode_->start_decode([this, &file, info = std::move(info)](auto type, auto ptr, auto len) {
            if (type == VideoDecoder::MediaType::Video)
            {
                QImage image((uchar*)ptr, info.video_width, info.video_height, QImage::Format_RGB32);
                emit imageReceived(image.copy());
            }
            return true;
        });
    }
    else {
        setText("error open");
    }
}

void FfplayWidget::paintImage(const QImage& image)
{
    setPixmap(QPixmap::fromImage(image));
}

