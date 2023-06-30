#include "VideoSendTest.h"
#include "VideoSampleDialog.h"
#include "cfte_video_fmt1.hpp"
#include "ff_capture.h"
#include "ff_encoder.h"
#include <QApplication>
#include <QComboBox>
#include <QDataStream>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QNetworkDatagram>
#include <QSettings>
#include <QSpinBox>
#include <boost/endian/conversion.hpp>
#include <chrono>
#include <execution>

#include <objbase.h>
#include <windows.h>

using namespace boost::endian;
using namespace std::literals;

static QString CONFIG_FILE = QDir::homePath() + "/.atom/video_send_test.bcfg";
static int WIDTH = 400;
static int HEIGHT = 200;
static int FRAMERATE = 10;
static auto OUTPUT_INTERVAL = 10ms;

int main(int argc, char **argv)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    avdevice_register_all();
    QApplication a(argc, argv);

    a.setAttribute(Qt::AA_DisableWindowContextHelpButton);
    VideoSendTest d;
    d.show();
    a.exec();

    CoUninitialize();
    return 0;
}

VideoSendTest::VideoSendTest(QWidget *parent)
    : QDialog(parent)
    , outputTimer_(io_)
{
    displayTimer_.setSingleShot(false);
    displayTimer_.setInterval(1000 / FRAMERATE);

    ui_.setupUi(this);
    loadLastConfig();

    ui_.Channel->setValue(form_.channel);
    ui_.Format->setCurrentIndex(form_.format);
    ui_.Port->setValue(form_.port);
    ui_.RtrChannel->setValue(form_.rtrchannel);
    ui_.SyncBytes->setCurrentText(QString::number(form_.synclen));
    ui_.SyncValue->setText(QString::number(form_.syncword, 16).toUpper());
    ui_.FrameBytes->setValue(form_.framelen);
    ui_.SfidBytes->setCurrentText(QString::number(form_.sfidlen));
    ui_.SfidMin->setValue(form_.sfidmin);
    ui_.SfidMax->setValue(form_.sfidmax);
    ui_.Reserved->setValue(form_.reservedlen);
    ui_.Endian->setCurrentIndex(form_.bigendian ? 0 : 1);
    ui_.ForwardIp->setText(form_.forwardIp);
    ui_.ForwardPort->setValue(form_.forwardPort);

    connect(&displayTimer_, &QTimer::timeout, this, &VideoSendTest::updateSampleTime);
    connect(ui_.Start, &QPushButton::clicked, this, &VideoSendTest::ctrlSend);
}

VideoSendTest::~VideoSendTest()
{
}

void VideoSendTest::ctrlSend(bool checked)
{
    if (checked)
    {
        ui_.Form->setDisabled(true);

        form_.channel = ui_.Channel->value();
        form_.format = ui_.Format->currentIndex();
        form_.port = ui_.Port->value();
        form_.rtrchannel = ui_.RtrChannel->value();
        form_.synclen = ui_.SyncBytes->currentText().toUInt();
        form_.syncword = ui_.SyncValue->text().toULongLong(nullptr, 16);
        form_.framelen = ui_.FrameBytes->value();
        form_.sfidlen = ui_.SfidBytes->currentText().toUInt();
        form_.sfidmin = ui_.SfidMin->value();
        form_.sfidmax = ui_.SfidMax->value();
        form_.reservedlen = ui_.Reserved->value();
        form_.bigendian = ui_.Endian->currentIndex() == 0;
        form_.forwardIp = ui_.ForwardIp->text();
        form_.forwardPort = ui_.ForwardPort->value();

        saveCurrentConfig();

        doStart();
        ui_.Start->setText(QStringLiteral("停止"));
    }
    else
    {
        doStop();
        ui_.Start->setText(QStringLiteral("开始"));
        ui_.Form->setEnabled(true);
    }
}

void VideoSendTest::doStart()
{
    frameCount_ = 0;
    interrupted_ = false;

    displayTimer_.start();
    tmserver_ = std::make_unique<tm_server>();
    tmserver_->register_channel({
        form_.rtrchannel,
        32,
        (int)form_.framelen,
    });
    tmserver_->bind(ui_.Port->value());
    tmserver_->start();

    if (form_.format == 0)
    {
        cfte_video_fmt1::options fmt1_opts{
            .channels = (int)form_.channel,
            .bigendian = form_.bigendian,
            .syncword = (uint32_t)form_.syncword,
            .sfid_min = (uint16_t)form_.sfidmin,
            .sfid_max = (uint16_t)form_.sfidmax,
            .frame_len = (uint16_t)form_.framelen,
            .reserved_len = (uint16_t)form_.reservedlen,
        };
        fmt1_ = std::make_unique<cfte_video_fmt1>(fmt1_opts);
    }

    channels_.clear();
    for (auto i = 0; i < ui_.Channel->value(); ++i)
    {
        makeGrabChannel(i);
    }

    // outputTimer_.expires_after(OUTPUT_INTERVAL);
    // outputTimer_.async_wait([this](auto &&ec) {
    //    if (!ec)
    //    {
    //        outputPictures();
    //    }
    //});
    outThread_ = std::thread([this] {
        // io_.run();
        while (!interrupted_)
        {
            outputPictures();
            std::this_thread::sleep_for(10ms);
        }
    });

    threads_.clear();
    for (auto &gc : channels_)
    {
        threads_.emplace_back([&gc] {
            gc.grab->run();
        });
    }
}

void VideoSendTest::doStop()
{
    interrupted_ = true;
    outputTimer_.cancel();

    if (outThread_.joinable())
    {
        outThread_.join();
    }
    channels_.clear();
    for (auto &t : threads_)
    {
        if (t.joinable())
            t.join();
    }

    displayTimer_.stop();
    if (fmt1_)
    {
        fmt1_.reset();
    }
    if (tmserver_)
    {
        tmserver_->stop();
        tmserver_.reset();
    }
}

void VideoSendTest::updateSampleTime()
{
    auto now = QTime::currentTime().toString("hh:mm:ss.zzz");
    for (auto &&[i, d, g] : channels_)
    {
        d->setText(now);
    }
}

void VideoSendTest::outputPictures()
{
    if (tmserver_)
    {
        auto now = QDateTime::currentMSecsSinceEpoch();
        auto frame = fmt1_->make_sub_frame();
        tmserver_->push(form_.rtrchannel, now, std::make_shared<std::vector<uint8_t>>(frame));

        // outputTimer_.expires_after(OUTPUT_INTERVAL);
        // outputTimer_.async_wait([this](auto &&ec) {
        //    if (!ec)
        //    {
        //        outputPictures();
        //    }
        //});
    }
}

void VideoSendTest::saveCurrentConfig()
{
    QFile file(CONFIG_FILE);
    if (file.open(QFile::WriteOnly | QFile::Truncate))
    {
        QDataStream out(&file);
        out << form_.channel << form_.format << form_.port << form_.syncword << form_.synclen << form_.framelen << form_.sfidlen << form_.sfidmin
            << form_.sfidmax << form_.reservedlen << form_.bigendian << form_.rtrchannel << form_.forwardIp << form_.forwardPort;
        file.close();
    }
}

void VideoSendTest::loadLastConfig()
{
    QFile file(CONFIG_FILE);
    if (file.open(QFile::ReadOnly))
    {
        QDataStream in(&file);

        in >> form_.channel >> form_.format >> form_.port >> form_.syncword >> form_.synclen >> form_.framelen >> form_.sfidlen >> form_.sfidmin >>
            form_.sfidmax >> form_.reservedlen >> form_.bigendian >> form_.rtrchannel >> form_.forwardIp >> form_.forwardPort;

        file.close();
    }
}

void VideoSendTest::makeGrabChannel(int i)
{
    auto rtp_ip = form_.forwardIp.toStdString();
    auto rtp_port = form_.forwardPort + i * 10;
    auto dialog = std::make_unique<VideoSampleDialog>(i);
    dialog->setFixedSize(WIDTH, HEIGHT);
    dialog->move(0, i * HEIGHT);

    auto rect = dialog->geometry();
    auto ts_enc = std::make_shared<ff_encoder>("mpegts", "", rect.width(), rect.height(), FRAMERATE);
    ts_enc->on_mux_packet([i, this](auto &&buf, auto &&len) {
        fmt1_->push_channel_packet(i, buf, len);
    });
    ts_enc->on_enc_packet([i](auto &&pkt) {
        // printf("%d pts %lld\n", i, pkt->pts);
    });

    auto rtp_enc = std::make_shared<ff_encoder>("rtp_mpegts", std::format("rtp://{}:{}", rtp_ip, rtp_port), rect.width(), rect.height(), FRAMERATE);
    std::vector<std::shared_ptr<ff_encoder>> encoders{ ts_enc, rtp_enc };
    auto capture = std::make_unique<ff_capture>(std::array<int, 4>{ rect.x(), rect.y(), rect.width(), rect.height() }, FRAMERATE);
    capture->on_yuv_frame([i, encs = std::move(encoders)](auto &&yuv) {
        std::for_each(std::execution::par_unseq, encs.begin(), encs.end(), [yuv](auto &&it) {
            it->encode(yuv);
        });
    });

    channels_.emplace_back(i, std::move(dialog), std::move(capture));
    channels_[i].dialog->show();
}
