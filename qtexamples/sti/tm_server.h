#pragma once

#include "tcp_server.h"

using data_ptr = std::shared_ptr<std::vector<unsigned char>>;

class tm_server : public tcp_server
{
public:
    tm_server();
    ~tm_server() = default;

    struct channel
    {
        int id;
        int sword_len;  // in bits
        int frame_len;  // in bytes
        int block_num;  // ignored
    };
    void register_channel(const channel &ch);

    using tcp_server::bind;
    using tcp_server::is_running;
    using tcp_server::start;

    void stop() override;
    /**
     * @brief 放入遥测帧到缓存
     * @param[in] channel 遥测通道
     * @param[in] ms      帧对应的时间
     * @param[in] frame   遥测帧
     */
    void push(int channel, uint64_t ms, data_ptr frame);

private:
    void check_connection(sock_ptr socket);
    /**
     * @brief 读取数据处理函数
     * @param[in] ec 错误码
     * @param[in] bytes_transferred 接收的字节数
     * @param[in] socket 客户端标示
     */
    void read_handler(const boost::system::error_code &ec, size_t bytes_transferred, sock_ptr socket);

private:
    struct tm_server_impl;
    std::shared_ptr<tm_server_impl> impl_;
};

template <class T>
inline constexpr T SwapEndian32(T src)
{
    return 0 | ((src & 0x000000ff) << 24) | ((src & 0x0000ff00) << 8) | ((src & 0x00ff0000) >> 8) | ((src & 0xff000000) >> 24);
}