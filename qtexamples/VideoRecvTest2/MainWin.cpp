#include "MainWin.h"
#include "ff_decoder.h"
#include "ff_encoder.h"
#include "sti/cortex_sti_parser.h"
#include "ts_decode.hpp"
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTimeZone>
#include <boost/endian/conversion.hpp>
#include <boost/thread/sync_bounded_queue.hpp>
#include <execution>
#include <format>
#include <fstream>

static QString CONFIG_FILE = QDir::homePath() + "/.atom/video_recv_test.bcfg";

using namespace boost::endian;

constexpr static size_t HEAD_OFFSET = 0;
// constexpr static size_t HEAD_OFFSET = 8;
constexpr static size_t PCM_FRAME_LEN = 512;
constexpr static size_t SYNC_LEN = 4;
constexpr static size_t SFID_LEN = 2;
constexpr static size_t UPDATE_INTERVAL = 100;

using namespace boost::endian;

static bool is_idle_frame(const uint8_t *ptr)
{
    if (ptr == nullptr)
    {
        return true;
    }
    auto numbers = *(uint64_t *)ptr;

    return numbers == 0 || numbers == 0x5555'5555'5555'5555 || numbers == 0xFADE'FADE'FADE'FADE || numbers == 0xDEFA'DEFA'DEFA'DEFA ||
           numbers == 0xDEAD'DEAD'DEAD'DEAD || numbers == 0xADDE'ADDE'ADDE'ADDE;
}

static bool is_idle_frame(const Frame &frame)
{
    if (frame.payload.size() < 8)
    {
        return true;
    }
    return is_idle_frame(frame.payload.data() + frame.offset);
}

MainWin::MainWin()
    : QWidget()
    , timer_(new QTimer(this))
{
    ui_.setupUi(this);
    ui_.label_6->hide();
    ui_.ChannelEdit->hide();
    ui_.label_7->hide();
    ui_.TimeCodeEdit->hide();

    {
        // ui_.Title4->hide();
        // ui_.label_13->hide();
        // ui_.label_14->hide();
        // ui_.ForwardIpEdit->hide();
        // ui_.ForwardPortEdit->hide();
        // ui_.verticalLayout->removeItem(ui_.verticalLayout->takeAt(2));
    }
    ui_.VideoChannelsEdit->setMinimum(1);
    ui_.ReservedEdit->setMaximum(256);
    ui_.VideoChannelsEdit->setValue(3);
    playerLayout_ = new QGridLayout(ui_.Player);
    playerLayout_->setContentsMargins(0, 0, 0, 0);

    timer_->setSingleShot(false);

    tasks_ = {
        [this] {
            ui_.TmTime->setText(QDateTime::fromMSecsSinceEpoch(1000 * time_, QTimeZone::systemTimeZone()).toString("yyyy-MM-dd hh:mm:ss.zzz"));
            ui_.SFID->setText(QString::number(sfid_));
            ui_.FrameCount->setText(QString::number(frameCount_));
            ui_.Bytes->setText(QString::number(receivedBytes_));
        },
    };

    connect(timer_, &QTimer::timeout, this, &MainWin::updateDisplay);
    connect(ui_.Start, &QPushButton::clicked, this, [this](bool checked) {
        return checked ? start() : stop();
    });
    connect(this, &MainWin::imageReceived, this, &MainWin::paintImage, Qt::QueuedConnection);

    loadLastConfig();
}

MainWin::~MainWin()
{
    stop();
}

void MainWin::start()
{
    saveCurrentConfig();
    while (playerLayout_->count())
    {
        auto item = playerLayout_->takeAt(0);
        item->widget()->setParent(nullptr);
        item->widget()->deleteLater();
        delete item;
    }

    auto ip = ui_.IpEdit->text();
    auto port = ui_.PortEdit->value();
    auto channel = ui_.ChannelEdit->value();
    auto timeCode = ui_.TimeCodeEdit->value();
    auto forwardIp = ui_.ForwardIpEdit->text();
    auto forwardPort = ui_.ForwardPortEdit->value();

    reserved_ = ui_.ReservedEdit->value();
    videoFmt_ = ui_.VideoFmt->currentIndex();
    channels_ = ui_.VideoChannelsEdit->value();
    bigendian_ = ui_.EndianEdit->currentIndex() == 1;
    id2channel_.clear();

    for (auto i = 0; i < channels_; ++i)
    {
        auto player = new QLabel(this);
        playerLayout_->addWidget(player, i / 2, i % 2);

        auto decode = std::make_unique<ff_decoder>();
        auto rtpurl = std::format("rtp://{}:{}", forwardIp.toStdString(), forwardPort + i * 10);
        auto fwd = std::make_shared<ff_encoder>("rtp_mpegts", rtpurl, 400, 200, 10);
        decode->on_frame([i, fwd](auto &&frame) {
            // printf("%d %lld\n", i, frame->pts);
            fwd->encode(frame);
        });
        decode->on_bgra_picture([idx = i, player, this](auto &&buf, auto &&len, int width, int height) {
            QImage image((uchar *)buf, width, height, QImage::Format_RGB32);
            emit imageReceived(idx, image.copy());
        });

        id2channel_[i].decode_thread = std::thread([this, i] {
            id2channel_[i].decode->run();
        });
        id2channel_[i].player = player;
        id2channel_[i].decode = std::move(decode);
        id2channel_[i].rawfile = std::ofstream(std::format("tsfile_{}.raw", i), std::ios::binary | std::ios::trunc);
        id2channel_[i].tsfile = std::ofstream(std::format("tsfile_{}.ts", i), std::ios::binary | std::ios::trunc);
    }

    auto videoDataOffset = HEAD_OFFSET + SYNC_LEN + SFID_LEN + (videoFmt_ == 0 ? reserved_ : 0);
    auto parser = std::make_shared<cortex::cortex_sti_parser>((HEAD_OFFSET + PCM_FRAME_LEN) * 320);
    parser->set_tm_msg_callback_fun([offset = videoDataOffset, this](auto &&frame) {
        auto ptr = frame->data();
        auto time = (uint64_t)load_big_u32(ptr + 12) * 1e6 + load_big_u32(ptr + 16);
        time = QDateTime({ QDate::currentDate().year(), 1, 1 }).addMSecs(time / 1e3).toMSecsSinceEpoch();
        std::vector<uint8_t> payload(frame->begin() + 64, frame->end() - 4);

        Frame video_frame{ frameCount_++, (double)time / 1e3, std::move(payload), offset };
        if (!is_idle_frame(video_frame))
        {
            dispatchFrame(video_frame);
        }
    });
    parser->start();

    tmc_ = std::make_unique<cortex::crt_tm_client>();
    tmc_->set_config({ ip.toStdString(), (uint16_t)port, 0 });
    tmc_->set_tm_data_callback_fun([parser](auto &&begin, auto &&end) {
        parser->push_data(begin, end);
    });
    tmc_->set_error_log_callback_fun([](auto &&msg) {
        printf("tmc error: %s\n", msg.c_str());
    });
    tmc_->start();

    timer_->start(UPDATE_INTERVAL);
    ui_.Start->setText(QStringLiteral("停止"));
}

void MainWin::stop()
{
    if (tmc_)
    {
        tmc_->stop();
        tmc_.reset();
    }
    for (auto &[i, f] : id2channel_)
    {
        f.decode.reset();
        if (f.decode_thread.joinable())
        {
            f.decode_thread.join();
        }
        f.rawfile.close();
        f.tsfile.close();
    }
    id2channel_.clear();
    timer_->stop();
    time_ = 0;
    sfid_ = 0;
    frameCount_ = 0;
    receivedBytes_ = 0;
    ui_.Start->setText(QStringLiteral("开始"));
}

void MainWin::updateDisplay()
{
    for (auto &t : tasks_)
    {
        t();
    }
}

// static std::ofstream ts0("www_recv_0.ts", std::ios::binary | std::ios::trunc);

void MainWin::dispatchFrame(const Frame &frame)
{
    sfid_ = boost::endian::load_big_u16(frame.payload.data() + HEAD_OFFSET + SYNC_LEN);
    time_ = frame.time;
    receivedBytes_ += frame.payload.size();

    switch (videoFmt_)
    {
    case 0:
        doDispatchColumn(frame);
        break;
    case 1:
        doDispatchColumnContinus(frame);
        break;
    case 2:
        doDispatchRow(frame);
        break;
    case 3:
        doDispatchRowContinus(frame);
        break;
    }
}

void MainWin::doDispatchColumn(const Frame &frame)
{
    int index = 0;
    auto bytesPerChannel = (frame.payload.size() - frame.offset) / channels_;

    auto ptr = (char *)frame.payload.data();
    for (auto i = frame.offset; i < frame.payload.size(); i += 2)
    {
        auto index = ((i - frame.offset) / 2) % channels_;
        auto &chan = id2channel_[index];
        if (bigendian_)
        {
            std::copy(ptr + i, ptr + i + 2, std::back_inserter(chan.payload));
        }
        else
        {
            std::reverse_copy(ptr + i, ptr + i + 2, std::back_inserter(chan.payload));
        }
    }

    for (auto &[id, chan] : id2channel_)
    {
        chan.rawfile.write((char *)chan.payload.data(), chan.payload.size());
        if (!is_idle_frame(chan.payload.data()))
        {
            // if (id == 0)
            {
                chan.decode->push_bytes(chan.payload.data(), chan.payload.size());
            }
            chan.tsfile.write((char *)chan.payload.data(), chan.payload.size());
        }
        chan.payload.clear();
    }
}

void MainWin::doDispatchColumnContinus(const Frame &frame)
{
    auto bytesPerChannel = (frame.payload.size() - frame.offset) / channels_;
    auto ptr = (uint8_t *)frame.payload.data() + frame.offset;
    for (auto i = 0; i < channels_; ++i)
    {
        std::vector<uint8_t> line(ptr + i * bytesPerChannel, ptr + (i + 1) * bytesPerChannel);
        auto &chan = id2channel_[i];
        chan.rawfile.write((char *)line.data(), line.size());
        if (!is_idle_frame(line.data()))
        {
            // if (id == 0)
            {
                chan.decode->push_bytes(line.data(), line.size());
            }
            chan.tsfile.write((char *)line.data(), line.size());
        }
    }
}

void MainWin::doDispatchRow(const Frame &frame)
{
    if (sfid_ < reserved_)
    {
        return;
    }
    int index = (sfid_ - reserved_) % channels_;
    if (index < 0 || !id2channel_.contains(index))
    {
        return;
    }
    auto ptr = (uint8_t *)frame.payload.data() + frame.offset;
    auto len = frame.payload.size() - frame.offset;
    if (!bigendian_)
    {
        std::reverse(ptr, ptr + len);
    }

    auto &chan = id2channel_.at(index);
    chan.rawfile.write((char *)ptr, len);
    if (!is_idle_frame(ptr))
    {
        // if (id == 0)
        {
            chan.decode->push_bytes(ptr, len);
        }
        chan.tsfile.write((char *)ptr, len);
    }
    // chan.payload.clear();
}

void MainWin::doDispatchRowContinus(const Frame &frame)
{
    if (sfid_ < reserved_)
    {
        return;
    }
    auto rows_per_channel = (32 - reserved_) / channels_;
    int index = (sfid_ - reserved_) / rows_per_channel;
    if (!id2channel_.contains(index))
    {
        return;
    }
    auto ptr = (uint8_t *)frame.payload.data() + frame.offset;
    auto len = frame.payload.size() - frame.offset;
    if (!bigendian_)
    {
        std::reverse(ptr, ptr + len);
    }

    auto &chan = id2channel_.at(index);
    chan.rawfile.write((char *)ptr, len);

    if (is_idle_frame(ptr))
    {
        return;
    }

    chan.decode->push_bytes(ptr, len);
    chan.tsfile.write((char *)ptr, len);
}

void MainWin::paintImage(int idx, const QImage &image)
{
    if (!tmc_) return;
    // auto size = ui_.Player->size();
    auto &chan = id2channel_[idx];
    chan.player->setPixmap(QPixmap::fromImage(image).scaled(chan.player->size()));
}

void MainWin::saveCurrentConfig()
{
    form_.ip = ui_.IpEdit->text();
    form_.port = ui_.PortEdit->value();
    form_.video_mode = ui_.VideoFmt->currentIndex();
    form_.video_count = ui_.ChannelEdit->value();
    form_.reserved_row_or_col = ui_.ReservedEdit->value();
    form_.bigendian = ui_.EndianEdit->currentIndex();

    QFile file(CONFIG_FILE);
    if (file.open(QFile::WriteOnly | QFile::Truncate))
    {
        QDataStream out(&file);
        out << form_.ip << form_.port << form_.video_mode << form_.video_count << form_.reserved_row_or_col << form_.bigendian;
        file.close();
    }
}

void MainWin::loadLastConfig()
{
    QFile file(CONFIG_FILE);
    if (file.open(QFile::ReadOnly))
    {
        QDataStream in(&file);
        in >> form_.ip >> form_.port >> form_.video_mode >> form_.video_count >> form_.reserved_row_or_col >> form_.bigendian;
        file.close();
    }

    ui_.IpEdit->setText(form_.ip);
    ui_.PortEdit->setValue(form_.port);
    ui_.VideoFmt->setCurrentIndex(form_.video_mode);
    ui_.ChannelEdit->setValue(form_.video_count);
    ui_.ReservedEdit->setValue(form_.reserved_row_or_col);
    ui_.EndianEdit->setCurrentIndex(form_.bigendian ? 1 : 0);
}
