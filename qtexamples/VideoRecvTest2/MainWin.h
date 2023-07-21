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
#include <fstream>
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
    std::ofstream rawfile;
    std::ofstream tsfile;
    // std::unique_ptr<ff_encoder> fwd{ nullptr };
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
    void doDispatchColumn(const Frame &);
    void doDispatchColumnContinus(const Frame &);
    void doDispatchRow(const Frame &);
    void doDispatchRowContinus(const Frame &);
    void paintImage(int idx, const QImage &image);
    void saveCurrentConfig();
    void loadLastConfig();

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
    int reserved_{ 2 };
    int videoFmt_{ 0 };

    std::atomic<double> time_{ 0 };
    std::atomic<uint16_t> sfid_{ 0 };
    std::atomic<size_t> frameCount_{ 0 };
    std::atomic<size_t> receivedBytes_{ 0 };

    std::map<size_t, VideoChannel> id2channel_;
    std::unique_ptr<cortex::crt_tm_client> tmc_;

    struct VideoRecvConfig
    {
        QString ip{ "127.0.0.1" };
        int port{ 3070 };
        int video_mode{ 0 };
        int video_count{ 3 };
        int reserved_row_or_col{ 2 };
        bool bigendian{ false };
    } form_;
};
