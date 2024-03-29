#include "MainWin.h"
#include "SplitFrame.h"
#include "ff_decoder.h"
#include "ff_encoder.h"
#include "sti_reader.h"
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QTimeZone>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/thread/sync_bounded_queue.hpp>
#include <execution>
#include <fmt/format.h>
#include <format>
#include <fstream>

using namespace boost::endian;
static QString CONFIG_FILE = QDir::homePath() + "/.atom/video_recv_test.bcfg";
constexpr static size_t HEAD_OFFSET = 0;
constexpr static size_t UPDATE_INTERVAL = 100;

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
    : QFrame()
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
    ui_.ReservedEdit->setMaximum(4096);
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
    connect(ui_.SaveAs, &QPushButton::clicked, this, [this]() {
        auto path = QFileDialog::getSaveFileName(this, QStringLiteral("保存配置文件"), {}, "*.bcfg");
        if (path.isEmpty()) return;
        saveCurrentConfig(path);
    });

    connect(ui_.CurrentConfig, &QPushButton::clicked, this, [this] {
        auto path = QFileDialog::getOpenFileName(this, QStringLiteral("加载配置文件"), "", "(*.bcfg)");
        if (path.isEmpty()) return;
        loadSpecifiedConfig(path);
        ui_.CurrentConfig->setProperty("path", path);
        ui_.CurrentConfig->setText(QFileInfo(path).baseName());
    });
    connect(this, &MainWin::imageReceived, this, &MainWin::paintImage, Qt::QueuedConnection);

    loadSpecifiedConfig();
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

    id2channel_.clear();
    auto bufferCapacity = std::pow(2, form_.parseCache) * 1024;
    for (auto i = 0; i < form_.videoChannelCount; ++i)
    {
        auto player = new QLabel(this);
        playerLayout_->addWidget(player, i / 2, i % 2);

        auto decode = std::make_unique<ff_decoder>(bufferCapacity);
        auto rtpurl = std::format("rtp://{}:{}", form_.forwardIp.toStdString(), form_.forwardPort + i * 10);
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

    auto videoDataOffset = HEAD_OFFSET + form_.syncBytes + form_.sfidBytes + (form_.videoMode == 0 ? form_.videoReserved : 0);
    // auto parser = std::make_shared<cortex::cortex_sti_parser>((HEAD_OFFSET + form_.frameBytes) * bufferCapacity);
    // parser->set_tm_msg_callback_fun([offset = videoDataOffset, this](auto &&frame) {
    //    auto ptr = frame->data();
    //    auto time = (uint64_t)load_big_u32(ptr + 12) * 1e6 + load_big_u32(ptr + 16);
    //    time = QDateTime({ QDate::currentDate().year(), 1, 1 }).addMSecs(time / 1e3).toMSecsSinceEpoch();
    //    std::vector<uint8_t> payload(frame->begin() + 64, frame->end() - 4);

    //    Frame video_frame{ frameCount_++, (double)time / 1e3, std::move(payload), offset };
    //    if (!is_idle_frame(video_frame))
    //    {
    //        dispatchFrame(video_frame);
    //    }
    //});
    // parser->start();

    // tmc_ = std::make_unique<cortex::crt_tm_client>();
    // tmc_->set_config({ form_.receiveIp.toStdString(), (uint16_t)form_.receivePort, 0 });
    // tmc_->set_tm_data_callback_fun([parser](auto &&begin, auto &&end) {
    //    parser->push_data(begin, end);
    //});
    // tmc_->set_error_log_callback_fun([](auto &&msg) {
    //    printf("tmc error: %s\n", msg.c_str());
    //});
    // tmc_->start();

    interrupted_ = false;
    client_thread_ = std::thread([this, ip = form_.receiveIp.toStdString(), port = (uint16_t)form_.receivePort, ch = 0] {
        startReceiveTm(ip, port, ch);
    });

    timer_->start(UPDATE_INTERVAL);
    ui_.ConfigFrame->setDisabled(true);
    ui_.Start->setText(QStringLiteral("停止"));
}

void MainWin::stop()
{
    interrupted_ = true;
    if (client_thread_.joinable()) client_thread_.join();

    for (auto &[i, f] : id2channel_)
    {
        f.decode.reset();
        if (f.decode_thread.joinable()) f.decode_thread.join();
        f.rawfile.close();
        f.tsfile.close();
    }
    id2channel_.clear();
    timer_->stop();
    time_ = 0;
    sfid_ = 0;
    frameCount_ = 0;
    receivedBytes_ = 0;

    ui_.ConfigFrame->setEnabled(true);
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
    if (form_.sfidIsBigEndian)
        sfid_ = boost::endian::load_big_u16(frame.payload.data() + HEAD_OFFSET + form_.syncBytes);
    else
        sfid_ = boost::endian::load_little_u16(frame.payload.data() + HEAD_OFFSET + form_.syncBytes);
    time_ = frame.time;
    receivedBytes_ += frame.payload.size();

    switch (form_.videoMode)
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
    // int index = 0;
    // auto bytesPerChannel = (frame.payload.size() - frame.offset) / channels_;

    // auto ptr = (char *)frame.payload.data();
    // for (auto i = frame.offset; i < frame.payload.size(); i += 2)
    //{
    //    auto index = ((i - frame.offset) / 2) % channels_;
    //    auto &chan = id2channel_[index];
    //    if (bigendian_)
    //    {
    //        std::copy(ptr + i, ptr + i + 2, std::back_inserter(chan.payload));
    //    }
    //    else
    //    {
    //        std::reverse_copy(ptr + i, ptr + i + 2, std::back_inserter(chan.payload));
    //    }
    //}

    // for (auto &[id, chan] : id2channel_)
    //{
    //    chan.rawfile.write((char *)chan.payload.data(), chan.payload.size());
    //    if (!is_idle_frame(chan.payload.data()))
    //    {
    //        // if (id == 0)
    //        {
    //            chan.decode->push_bytes(chan.payload.data(), chan.payload.size());
    //        }
    //        chan.tsfile.write((char *)chan.payload.data(), chan.payload.size());
    //    }
    //    chan.payload.clear();
    //}
    split_frame_column_cross(frame, form_.videoChannelCount, form_.videoDataIsBigEndian, [this](auto &&idx, auto &&payload) {
        if (!id2channel_.contains(idx)) return;

        auto ptr = payload.data();
        auto len = payload.size();
        auto &&chan = id2channel_[idx];
        chan.rawfile.write((char *)ptr, len);
        if (!is_idle_frame(ptr))
        {
            chan.decode->push_bytes(ptr, len);
            chan.tsfile.write((char *)ptr, len);
        }
        chan.payload.clear();
    });
}

void MainWin::doDispatchColumnContinus(const Frame &frame)
{
    auto bytesPerChannel = (frame.payload.size() - frame.offset) / form_.videoChannelCount;
    auto ptr = (uint8_t *)frame.payload.data() + frame.offset;
    for (auto i = 0; i < form_.videoChannelCount; ++i)
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
    if (sfid_ < form_.videoReserved)
    {
        return;
    }
    int index = (sfid_ - form_.videoReserved) % form_.videoChannelCount;
    if (index < 0 || !id2channel_.contains(index))
    {
        return;
    }
    auto ptr = (uint8_t *)frame.payload.data() + frame.offset;
    auto len = frame.payload.size() - frame.offset;
    if (!form_.videoDataIsBigEndian)
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
    if (sfid_ < form_.videoReserved)
    {
        return;
    }
    auto rows_per_channel = (32 - form_.videoReserved) / form_.videoChannelCount;
    int index = (sfid_ - form_.videoReserved) / rows_per_channel;
    if (!id2channel_.contains(index))
    {
        return;
    }
    auto ptr = (uint8_t *)frame.payload.data() + frame.offset;
    auto len = frame.payload.size() - frame.offset;
    if (!form_.videoDataIsBigEndian)
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
    if (interrupted_) return;
    // if (!tmc_) return;
    // auto size = ui_.Player->size();
    auto &chan = id2channel_[idx];
    chan.player->setPixmap(QPixmap::fromImage(image).scaled(chan.player->size()));
}

void MainWin::saveCurrentConfig(const QString &path)
{
    form_.receiveIp = ui_.IpEdit->text();
    form_.receivePort = ui_.PortEdit->value();
    form_.tmChannel = ui_.ChannelEdit->value();
    form_.tmTimeCode = ui_.TimeCodeEdit->value();
    form_.frameBytes = ui_.FrameLen->text().toInt();
    form_.syncBytes = ui_.SyncLen->text().toInt();
    form_.sfidBytes = ui_.SfidLen->text().toInt();
    form_.sfidIsBigEndian = ui_.SfidEndian->currentIndex() == 1;
    form_.videoMode = ui_.VideoFmt->currentIndex();
    form_.videoChannelCount = ui_.VideoChannelsEdit->value();
    form_.videoReserved = ui_.ReservedEdit->value();
    form_.videoDataIsBigEndian = ui_.EndianEdit->currentIndex() == 1;
    form_.forwardIp = ui_.ForwardIpEdit->text();
    form_.forwardPort = ui_.ForwardPortEdit->value();
    form_.parseCache = ui_.ParseCacheMode->currentIndex();

    QFile file(path.isEmpty() ? CONFIG_FILE : path);
    if (file.open(QFile::WriteOnly | QFile::Truncate))
    {
        QDataStream out(&file);
        out << form_.receiveIp << form_.receivePort << form_.videoMode << form_.videoChannelCount << form_.videoReserved << form_.sfidIsBigEndian;
        out << form_.tmChannel << form_.tmTimeCode << form_.frameBytes << form_.syncBytes << form_.sfidBytes << form_.videoDataIsBigEndian << form_.forwardIp
            << form_.forwardPort << form_.parseCache;
        file.close();
    }
}

void MainWin::loadSpecifiedConfig(const QString &path)
{
    QFile file(path.isEmpty() ? CONFIG_FILE : path);
    if (file.open(QFile::ReadOnly))
    {
        QDataStream in(&file);
        in >> form_.receiveIp >> form_.receivePort >> form_.videoMode >> form_.videoChannelCount >> form_.videoReserved >> form_.sfidIsBigEndian;
        in >> form_.tmChannel >> form_.tmTimeCode >> form_.frameBytes >> form_.syncBytes >> form_.sfidBytes >> form_.videoDataIsBigEndian >> form_.forwardIp >>
            form_.forwardPort >> form_.parseCache;
        file.close();
    }

    ui_.IpEdit->setText(form_.receiveIp);
    ui_.PortEdit->setValue(form_.receivePort);
    ui_.ChannelEdit->setValue(form_.tmChannel);
    ui_.TimeCodeEdit->setValue(form_.tmTimeCode);
    ui_.FrameLen->setText(QString::number(form_.frameBytes));
    ui_.SyncLen->setText(QString::number(form_.syncBytes));
    ui_.SfidLen->setText(QString::number(form_.sfidBytes));
    ui_.SfidEndian->setCurrentIndex(form_.sfidIsBigEndian ? 1 : 0);
    ui_.VideoFmt->setCurrentIndex(form_.videoMode);
    ui_.VideoChannelsEdit->setValue(form_.videoChannelCount);
    ui_.ReservedEdit->setValue(form_.videoReserved);
    ui_.EndianEdit->setCurrentIndex(form_.videoDataIsBigEndian ? 1 : 0);
    ui_.ForwardIpEdit->setText(form_.forwardIp);
    ui_.ForwardPortEdit->setValue(form_.forwardPort);
    ui_.ParseCacheMode->setCurrentIndex(form_.parseCache);
}

void MainWin::startReceiveTm(const std::string &ip, uint16_t port, uint16_t channel)
{
    boost::asio::io_context io;
    auto offset = HEAD_OFFSET + form_.syncBytes + form_.sfidBytes + (form_.videoMode == 0 ? form_.videoReserved : 0);
    sti_reader r(io, ip, port);
    r.run(channel, 0, [this, offset](auto &&msg) {
        auto ptr = (uint8_t *)msg.data();
        auto time = (uint64_t)load_big_u32(ptr + 12) * 1e6 + load_big_u32(ptr + 16);
        time = QDateTime({ QDate::currentDate().year(), 1, 1 }).addMSecs(time / 1e3).toMSecsSinceEpoch();
        std::vector<uint8_t> payload(msg.begin() + 64, msg.end() - 4);
        Frame video_frame{ frameCount_++, (double)time / 1e3, std::move(payload), offset };
        // if (!is_idle_frame(video_frame))
        {
            dispatchFrame(video_frame);
        }
        return !interrupted_;
    });
    io.run();
}
