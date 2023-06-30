#include "MainWin.h"
#include "ff_decoder.h"
#include "ff_encoder.h"
#include "sti/cortex_sti_parser.h"
#include "ts_decode.hpp"
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

using namespace boost::endian;

constexpr static size_t UPDATE_INTERVAL = 100;
constexpr static size_t SFID_OFFSET = 4;

using namespace boost::endian;

static bool is_idle_frame(uint8_t *ptr, size_t len)
{
    if (len < 8)
    {
        return true;
    }
    auto numbers = *(uint64_t *)ptr;

    return numbers == 0 || numbers == 0xFADE'FADE'FADE'FADE || numbers == 0xDEFA'DEFA'DEFA'DEFA || numbers == 0xDEAD'DEAD'DEAD'DEAD ||
           numbers == 0xADDE'ADDE'ADDE'ADDE;
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
}

MainWin::~MainWin()
{
    stop();
}

void MainWin::start()
{
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
    auto videoDataOffset = sizeof(uint32_t) + sizeof(uint16_t) + ui_.ReservedEdit->value();
    auto forwardIp = ui_.ForwardIpEdit->text();
    auto forwardPort = ui_.ForwardPortEdit->value();

    channels_ = ui_.VideoChannelsEdit->value();
    bigendian_ = ui_.EndianEdit->currentIndex() == 1;
    id2channel_.clear();
    for (auto i = 0; i < channels_; ++i)
    {
        auto player = new QLabel(this);
        playerLayout_->addWidget(player, i / 2, i % 2);

        auto rtpurl = std::format("rtp://{}:{}", forwardIp.toStdString(), forwardPort + i * 10);
        auto rtppush = std::make_shared<ff_encoder>("rtp_mpegts", rtpurl, 400, 200, 10);
        auto decode = std::make_unique<ff_decoder>();
        decode->on_frame([i, rtppush = std::move(rtppush)](auto &&frame) {
            rtppush->encode(frame);
            printf("%d %lld\n", i, frame->pts);
        });
        decode->on_bgra_picture([i, player, this](auto &&buf, auto &&len, int width, int height) {
            QImage image((uchar *)buf, width, height, QImage::Format_RGB32);
            emit imageReceived(i, image.copy());
        });

        id2channel_[i].decode_thread = std::thread([this, i] {
            id2channel_[i].decode->run();
        });
        id2channel_[i].player = player;
        id2channel_[i].decode = std::move(decode);
    }

    auto parser = std::make_shared<cortex::cortex_sti_parser>(512 * 1000);
    parser->set_tm_msg_callback_fun([offset = videoDataOffset, this](auto &&frame) {
        auto ptr = frame->data();
        auto time = load_big_u32(ptr + 12) * 1e3 + load_big_u32(ptr + 16);
        time = QDateTime({ QDate::currentDate().year(), 1, 1 }).addMSecs(time).toMSecsSinceEpoch();
        std::vector<uint8_t> payload(frame->begin() + 64, frame->end() - 4);
        if (!is_idle_frame(payload.data() + offset, payload.size()))
        {
            dispatchFrame({ frameCount_++, (double)time / 1e3, std::move(payload), offset });
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

static std::ofstream ts0("www_recv_0.ts", std::ios::binary | std::ios::trunc);
void MainWin::dispatchFrame(const Frame &frame)
{
    sfid_ = boost::endian::load_big_u16(frame.payload.data() + sizeof(uint32_t));
    time_ = frame.time;
    receivedBytes_ += frame.payload.size();

    int index = 0;
    auto bytesPerChannel = (frame.payload.size() - frame.offset) / channels_;

    auto ptr = (char *)frame.payload.data();
    for (auto i = frame.offset; i < frame.payload.size(); i += 2)
    {
        auto index = (i / 2) % channels_;
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

    for (auto &&[id, chan] : id2channel_)
    {
        chan.decode->push_bytes(chan.payload.data(), chan.payload.size());
        chan.payload.clear();
    }
    // std::for_each(std::execution::par_unseq, id2channel_.begin(), id2channel_.end(), [](auto &&it) {
    //});
}

void MainWin::paintImage(int idx, const QImage &image)
{
    if (!tmc_) return;
    auto &chan = id2channel_[idx];
    chan.player->setPixmap(QPixmap::fromImage(image));
}
