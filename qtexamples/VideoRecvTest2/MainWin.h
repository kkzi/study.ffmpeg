#pragma once

#include "Frame.h"
#include "sti/cortex_tm_client.h"
#include "ui_MainWin.h"
#include <QList>
#include <QPixmap>
#include <QQueue>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <functional>
#include <memory>
#include <mutex>

struct SwsContext;
class ff_decoder;
class ff_encoder;
class QGridLayout;

struct VideoChannel
{
    QLabel *player;
    std::unique_ptr<ff_decoder> decode{ nullptr };
    std::thread decode_thread;
    std::vector<uint8_t> payload;
};

class MainWin : public QWidget
{
    Q_OBJECT

public:
    MainWin();
    ~MainWin();

private:
    void start();
    void stop();
    void updateDisplay();
    void dispatchFrame(const Frame &);
    void paintImage(int idx, const QImage &image);

signals:
    void statusChanged(const QString &);
    void imageReceived(int idx, const QImage &image);

private:
    Ui::MainWin ui_;
    QGridLayout *playerLayout_{ nullptr };
    QTimer *timer_;
    QList<std::function<void()>> tasks_;

    int channels_{ 2 };
    bool bigendian_{ false };

    std::atomic<double> time_{ 0 };
    std::atomic<uint16_t> sfid_{ 0 };
    std::atomic<size_t> frameCount_{ 0 };
    std::atomic<size_t> receivedBytes_{ 0 };

    std::map<size_t, VideoChannel> id2channel_;
    std::unique_ptr<cortex::crt_tm_client> tmc_;
};
