#include "rpc_server.h"

template <class T>
inline constexpr T SwapEndian32(T src)
{
    return 0 | ((src & 0x000000ff) << 24) | ((src & 0x0000ff00) << 8) | ((src & 0x00ff0000) >> 8) | ((src & 0xff000000) >> 24);
}

rpc_server::rpc_server()
    : readBuf_(new std::vector<uint8_t>(1500))
    , parseBuf_(new std::vector<uint8_t>(1500))
{
    parseBuf_->reserve(1500);
    tcp_server::bind_new_connection_handler([&](sock_ptr socket) {
        if (client_)
            return;
        client_ = socket;
        check_connection();
    });
}

rpc_server::~rpc_server()
{
}

void rpc_server::stop()
{
    tcp_server::stop();
    //停止时主动释放资源
    if (client_ && client_->is_open())
    {
        client_->cancel();
    }
}

void rpc_server::response(int type, const data_ptr data)
{
    int byteAligned = 4;
    int offsetNum = std::ceil((double)data->size() / byteAligned);
    //回复消息
    std::vector<int> replyMsg(6 + offsetNum);
    replyMsg[0] = SwapEndian32(1234567890);
    replyMsg[1] = SwapEndian32(replyMsg.size() * 4);
    replyMsg[4] = SwapEndian32(-1234567890);
    replyMsg[2] = SwapEndian32(1);
    replyMsg[3] = SwapEndian32(type);
    replyMsg[4] = SwapEndian32(int(data->size()));
    memcpy(&replyMsg[5], data->data(), data->size());
    replyMsg[6 + offsetNum - 1] = SwapEndian32(-1234567890);
    client_->write_some(boost::asio::buffer(replyMsg));
}

void rpc_server::check_connection()
{
    client_->async_read_some(boost::asio::buffer(*readBuf_),
        boost::bind(&rpc_server::read_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

inline int get_value(const uint8_t *v)
{
    return *reinterpret_cast<const int *>(v);
}

void rpc_server::read_handler(const boost::system::error_code &ec, size_t bytes_transferred)
{
    constexpr int head = SwapEndian32(1234567890);
    constexpr int tail = SwapEndian32(-1234567890);
    if (ec)
    {
        if (client_->is_open())
        {
            client_->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
            client_->close();
        }
        client_ = nullptr;
    }
    else
    {
        if (bytes_transferred > 0)
        {
            //拷贝到解析缓存末尾
            size_t size = parseBuf_->size();
            parseBuf_->resize(size + bytes_transferred);
            memcpy(parseBuf_->data() + size, readBuf_->data(), bytes_transferred);

            size_t offset = 0;
            size = parseBuf_->size();
            uint8_t *dataPtr = parseBuf_->data();
            //一个消息至少有20字节
            while (size >= 20)
            {
                //匹配消息头
                if (get_value(dataPtr + offset) != head)
                {
                    size_t bufSize = parseBuf_->size();
                    //搜索消息头
                    while (offset < bufSize - 3 && get_value(dataPtr + offset) != head)
                        ++offset;
                    if (offset == bufSize - 3)
                    {
                        //未搜到消息头，丢弃数据，保留末尾3个字节
                        memcpy(dataPtr, dataPtr + offset, 3);
                        parseBuf_->resize(3);
                        // qDebug(u8"暂时没找到下一个消息头，已丢弃%d字节", offset);
                        break;
                    }
                    // qDebug(u8"查找到下一个消息头，已丢弃%d字节", offset);
                    size = bufSize - offset;
                    //如果剩余字节小于20，无法组成消息
                    if (size < 20)
                        break;
                }
                //消息头正确，读取消息大小
                int msgSize = SwapEndian32(get_value(dataPtr + offset + 4));
                if (msgSize < 20)
                {
                    // emit errorTips(QStringLiteral("消息大小错误!"));

                    int *ptr = (int *)(dataPtr + offset);
                    //回复消息
                    std::vector<int> replyMsg(5);
                    replyMsg[0] = SwapEndian32(1234567890);
                    replyMsg[1] = SwapEndian32(20);
                    replyMsg[4] = SwapEndian32(-1234567890);
                    replyMsg[2] = ptr[2];
                    replyMsg[3] = SwapEndian32(1);  // Bad syntax
                    client_->write_some(boost::asio::buffer(replyMsg));

                    offset += 4;
                    size -= 4;
                    continue;
                }
                //消息未接收完
                if (size < msgSize)
                {
                    if (offset > 0)
                    {
                        memcpy(dataPtr, dataPtr + offset, size);
                        parseBuf_->resize(size);
                        offset = 0;
                    }
                    break;
                }
                //检查消息尾
                if (get_value(dataPtr + offset + msgSize - 4) != tail)
                {
                    // emit errorTips(QStringLiteral("消息尾错误!"));

                    int *ptr = (int *)(dataPtr + offset);
                    //回复消息
                    std::vector<int> replyMsg(5);
                    replyMsg[0] = SwapEndian32(1234567890);
                    replyMsg[1] = SwapEndian32(20);
                    replyMsg[4] = SwapEndian32(-1234567890);
                    replyMsg[2] = ptr[2];
                    replyMsg[3] = SwapEndian32(1);  // Bad syntax
                    client_->write_some(boost::asio::buffer(replyMsg));

                    offset += 4;
                    size -= 4;
                    continue;
                }
                //消息正确

                int *ptr = (int *)(dataPtr + offset);

                auto type = SwapEndian32(ptr[3]);
                auto dlen = SwapEndian32(ptr[4]);

                //回复消息
                std::vector<int> replyMsg(5);
                replyMsg[0] = SwapEndian32(1234567890);
                replyMsg[1] = SwapEndian32(20);
                replyMsg[4] = SwapEndian32(-1234567890);
                replyMsg[2] = ptr[2];

                constexpr auto DATA_OFFSET = 5;

                replyMsg[3] = SwapEndian32(0);
                client_->write_some(boost::asio::buffer(replyMsg));

                if (xfer_)
                {
                    data_ptr data(new std::vector<uint8_t>(dataPtr + offset + DATA_OFFSET * sizeof(int), dataPtr + offset + DATA_OFFSET * sizeof(int) + dlen));
                    xfer_(type, data);
                }

                offset += msgSize;
                size -= msgSize;
                if (size < 1024 || offset > 2024)
                {
                    memcpy(dataPtr, dataPtr + offset, size);
                    parseBuf_->resize(size);
                    offset = 0;
                }
            }
        }
        check_connection();
    }
}
