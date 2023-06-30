#pragma once


#include <QImage>
#include <QLabel>
#include <QString>
#include <memory>


class VideoDecoder;


class FfplayWidget : public QLabel
{
    Q_OBJECT

public:
    FfplayWidget(QWidget* parent = nullptr);

public:
    void open(const QString& url);

private:
    void paintImage(const QImage&);

signals:
    void imageReceived(const QImage& image);

private:
    std::shared_ptr<VideoDecoder> decode_{ nullptr };
};


