#pragma once

#include "cfte_video_fmt1.hpp"
#include "ff_capture.h"
#include "sti/tm_server.h"
#include "ui_VideoSendTest.h"
#include <QByteArray>
#include <QDialog>
#include <QImage>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QUdpSocket>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <memory>

class VideoSampleDialog;

class VideoSendTest : public QDialog
{
    Q_OBJECT;

public:
    VideoSendTest(QWidget *parent = nullptr);
    ~VideoSendTest();

private:
    void ctrlSend(bool checked);
    void doStart();
    void doStop();
    void updateSampleTime();
    void outputPictures();

    void saveCurrentConfig();
    void loadLastConfig();

private:
    Ui::VideoSendTest ui_;
    int lastChannel_{ 2 };

    struct GrabChannel
    {
        int index{ 0 };
        std::unique_ptr<VideoSampleDialog> dialog{ nullptr };
        std::unique_ptr<ff_capture> grab{ nullptr };
    };
    void makeGrabChannel(int i);

    std::vector<GrabChannel> channels_;
    std::vector<std::thread> threads_;

    QTimer displayTimer_;
    std::unique_ptr<tm_server> tmserver_{ nullptr };
    std::unique_ptr<cfte_video_fmt1> fmt1_{ nullptr };
    size_t frameCount_{ 0 };

    std::atomic<bool> interrupted_{ false };
    std::thread outThread_;
    boost::asio::io_context io_;

    struct
    {
        int channel{ 1 };
        int format{ 0 };
        int port{ 3070 };
        int rtrchannel{ 0 };
        size_t syncword{ 0xFE6B2840 };
        size_t synclen{ 4 };
        int framelen{ 512 };
        int sfidlen{ 2 };
        int sfidmin{ 0 };
        int sfidmax{ 31 };
        int reservedlen{ 2 };
        bool bigendian{ true };
        QString forwardIp{ "233.1.1.1" };
        int forwardPort{ 12300 };
    } form_;
};
