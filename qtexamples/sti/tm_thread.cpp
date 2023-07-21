#include "tm_thread.h"
#include <chrono>

using namespace std::chrono;

tm_thread::tm_thread(protocol_type ptype, tm_server::channel channel, sock_ptr socket)
    : ptype_(ptype)
    , channel_(channel)
    , socket_(socket)
{
}

tm_thread::~tm_thread()
{
}

void tm_thread::start()
{
    if (!is_running_)
    {
        is_running_ = true;
        thread_ = std::thread([=]() {
            if (ptype_ == protocol_type::CRT)
            {
                sim_crt();
            }
            else
            {
                sim_hdr();
            }
        });
    }
}

void tm_thread::stop()
{
    if (is_running_)
    {
        is_running_ = false;
        socket_->cancel();
        if (thread_.joinable()) thread_.join();
        queue_.reset();
    }
}

void tm_thread::push(uint64_t epoch_ms, data_ptr frame)
{
    if (is_running_ && socket_->is_open())
    {
        auto byteAligned = 0;
        if (ptype_ == protocol_type::CRT)
        {
            //遥测帧和块是32位对齐的
            byteAligned = 4;
        }
        else
        {
            //遥测帧和块是64位对齐的
            byteAligned = 8;
        }
        //帧长占用8字节的整数倍，不足补0
        int frameTakeBytes = std::ceil((double)frame->size() / byteAligned) * byteAligned;
        int offsetNum = (frameTakeBytes + sizeof(double)) / sizeof(int);

        time_point<system_clock> tp{ milliseconds(epoch_ms) };
        year_month_day t0(year_month_day{ std::chrono::floor<days>(tp) }.year(), month(1), day(1));
        auto ms = duration_cast<milliseconds>(tp - sys_days{ t0 }).count();

        //按照HDR格式把8字节时间加到帧尾
        data_ptr frameAddTime(new std::vector<unsigned char>(frameTakeBytes + sizeof(double)));
        std::copy(frame->begin(), frame->end(), frameAddTime->begin());
        //时间按照CODE0
        auto ptr = (unsigned int *)(frameAddTime->data());
        ptr[offsetNum - 2] = SwapEndian32(ms / 1000);  // s
        ptr[offsetNum - 1] = SwapEndian32(ms % 1000);  // ms

        if (!queue_.push(frameAddTime))
        {
            lost_count_++;
        }
    }
}

tm_server::channel tm_thread::channel() const
{
    return channel_;
}

void tm_thread::sim_crt()
{
    //遥测帧和块是32位对齐的(如果以字节为单位的帧或块长度不是4的倍数，则最后一个单词的lbs是零填充的)。
    int byteAligned = 4;
    int frameOffsetNum = std::ceil((double)channel_.frame_len / byteAligned) * byteAligned / sizeof(int);
    int msgOffsetNum = 17 + frameOffsetNum;
    int msgSize = msgOffsetNum * sizeof(int);

    while (is_running_)
    {
        data_ptr frameAddTime{ nullptr };
        if (queue_.pop(frameAddTime))
        {
            std::shared_ptr<std::vector<int>> replyMsg(new std::vector<int>(msgOffsetNum, 0));
            (*replyMsg)[0] = SwapEndian32(1234567890);
            (*replyMsg)[1] = SwapEndian32(msgSize);
            (*replyMsg)[2] = 0;

            //时间按照CODE0
            auto ptr = (unsigned int *)frameAddTime->data();
            (*replyMsg)[3] = ptr[frameOffsetNum];
            (*replyMsg)[4] = ptr[frameOffsetNum + 1];
            (*replyMsg)[5] = SwapEndian32(int(send_count_));
            (*replyMsg)[10] = SwapEndian32(channel_.frame_len);
            (*replyMsg)[11] = SwapEndian32(channel_.sword_len);
            (*replyMsg)[(size_t)msgOffsetNum - 1] = SwapEndian32(-1234567890);

            auto pos = (unsigned char *)replyMsg->data() + 64;
            memcpy(pos, frameAddTime->data(), frameAddTime->size() - sizeof(double));  //尾部8字节时间去掉

            boost::system::error_code ec;
            socket_->write_some(boost::asio::buffer(*replyMsg), ec);
            if (ec)
            {
                printf("%s\n", ec.message().c_str());
                break;
            }
            send_count_++;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void tm_thread::sim_hdr()
{
    //遥测帧和块是64位对齐的
    int byteAligned = 8;
    int frameTakeBytes = std::ceil((double)channel_.frame_len / byteAligned) * byteAligned;
    int blockTakeBytes = frameTakeBytes + 16;

    auto msgOffsetNum = 20 + (blockTakeBytes / sizeof(int)) * channel_.block_num;
    auto msgSize = msgOffsetNum * sizeof(int);

    while (is_running_)
    {
        if (queue_.read_available() >= channel_.block_num)
        {
            std::shared_ptr<std::vector<int>> replyMsg(new std::vector<int>(msgOffsetNum, 0));
            (*replyMsg)[0] = SwapEndian32(1234567890);
            (*replyMsg)[1] = SwapEndian32(msgSize);
            (*replyMsg)[2] = 0;
            (*replyMsg)[3] = SwapEndian32(channel_.id);
            (*replyMsg)[4] = SwapEndian32(4);  // 4 : Real time telemetry data
            (*replyMsg)[8] = SwapEndian32(channel_.sword_len);
            (*replyMsg)[9] = SwapEndian32(channel_.frame_len);
            (*replyMsg)[10] = SwapEndian32(1);  // 1 (in 64_bit words) Length of the time-tag field
            (*replyMsg)[12] = SwapEndian32(blockTakeBytes / 8);
            (*replyMsg)[13] = SwapEndian32(channel_.block_num);
            (*replyMsg)[15] = SwapEndian32(0xFFFFFFFF);
            (*replyMsg)[16] = SwapEndian32(0xFFFFFFFF);
            (*replyMsg)[17] = SwapEndian32(int(lost_count_));
            int usedPercent = (double)queue_.read_available() / 600 * 100;
            (*replyMsg)[18] = SwapEndian32(usedPercent);
            (*replyMsg)[(size_t)msgOffsetNum - 1] = SwapEndian32(-1234567890);

            auto index = 0;
            while (index < channel_.block_num)
            {
                data_ptr frameAddTime{ nullptr };
                if (queue_.pop(frameAddTime))
                {
                    auto pos = (unsigned char *)replyMsg->data() + 76 + index * frameAddTime->size();
                    memcpy(pos, frameAddTime->data(), frameAddTime->size());
                    index++;
                }
            }

            boost::system::error_code ec;
            socket_->write_some(boost::asio::buffer(*replyMsg), ec);
            if (ec)
            {
                printf("%s\n", ec.message().c_str());
                break;
            }
            send_count_ += channel_.block_num;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}