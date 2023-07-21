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
     * @brief ����ң��֡������
     * @param[in] channel ң��ͨ��
     * @param[in] ms      ֡��Ӧ��ʱ��
     * @param[in] frame   ң��֡
     */
    void push(int channel, uint64_t ms, data_ptr frame);

private:
    void check_connection(sock_ptr socket);
    /**
     * @brief ��ȡ���ݴ�����
     * @param[in] ec ������
     * @param[in] bytes_transferred ���յ��ֽ���
     * @param[in] socket �ͻ��˱�ʾ
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