#pragma once

#include "Frame.h"
#include "ff_decoder.h"
#include "ff_encoder.h"
#include "sti/cortex_tm_client.h"
#include "ui_MainWin.h"
#include <QList>
#include <QPixmap>
#include <QQueue>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>

struct SwsContext;
class QGridLayout;

struct VideoChannel
{
    QLabel *player;
    std::unique_ptr<ff_decoder> decode{ nullptr };
    std::thread decode_thread;
    std::vector<uint8_t> payload;
    std::ofstream rawfile;
    std::ofstream tsfile;
    // std::unique_ptr<ff_encoder> fwd{ nullptr };
};

struct VideoRecvConfig
{
    QString receiveIp;
    int receivePort{ 3070 };
    int tmChannel{ 0 };
    int tmTimeCode{ 0 };
    int frameBytes{ 512 };
    int syncBytes{ 4 };
    int sfidBytes{ 2 };
    bool sfidIsBigEndian{ true };
    int videoMode{ 0 };
    int videoChannelCount{ 3 };
    int videoReserved{ 2 };
    bool videoDataIsBigEndian{ false };
    QString forwardIp{ "235.1.1.1" };
    int forwardPort{ 32100 };

    int parseCache{ 320 };
};

class MainWin : public QFrame
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
    void doDispatchColumn(const Frame &);
    void doDispatchColumnContinus(const Frame &);
    void doDispatchRow(const Frame &);
    void doDispatchRowContinus(const Frame &);
    void paintImage(int idx, const QImage &image);
    void saveCurrentConfig(const QString &path = "");
    void loadSpecifiedConfig(const QString &path = "");

signals:
    void statusChanged(const QString &);
    void imageReceived(int idx, const QImage &image);

private:
    Ui::MainWin ui_;
    QGridLayout *playerLayout_{ nullptr };
    QTimer *timer_;
    QList<std::function<void()>> tasks_;

    std::atomic<double> time_{ 0 };
    std::atomic<uint16_t> sfid_{ 0 };
    std::atomic<size_t> frameCount_{ 0 };
    std::atomic<size_t> receivedBytes_{ 0 };

    std::map<size_t, VideoChannel> id2channel_;
    std::unique_ptr<cortex::crt_tm_client> tmc_;

    VideoRecvConfig form_;
};
