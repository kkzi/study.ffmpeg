#pragma once

#include "tcp_server.h"

using data_ptr = std::shared_ptr<std::vector<uint8_t>>;
using xfer_handler = std::function<void(int type, data_ptr data)>;

class rpc_server : public tcp_server
{
public:
    rpc_server();
    ~rpc_server();

    /**
     * @brief �󶨷���˼�����IP�Ͷ˿�
     */
    using tcp_server::bind;
    using tcp_server::is_running;
    using tcp_server::start;
    virtual void stop() override;

    void bind_xfer_handler(const xfer_handler &handler)
    {
        xfer_ = handler;
    }
    void response(int type, const data_ptr data);

private:
    void check_connection();
    /**
     * @brief ��ȡ���ݴ�����
     * @param[in] ec ������
     * @param[in] bytes_transferred ���յ��ֽ���
     */
    void read_handler(const boost::system::error_code &ec, size_t bytes_transferred);

private:
    sock_ptr client_{ nullptr };
    data_ptr readBuf_;
    data_ptr parseBuf_;
    xfer_handler xfer_;
};
