// Author     : Teo.Sun
// Date       : 2019-04-03
// Description: TCP server

#pragma once
#pragma warning(disable : 4005)

#include <memory>
#ifdef _MSC_VER
    #define _WIN32_WINNT 0x0501
#endif
#include <boost/asio.hpp>
#include <boost/thread.hpp>

using sock_ptr = std::shared_ptr<boost::asio::ip::tcp::socket>;
using thread_ptr = std::shared_ptr<boost::thread>;
using new_connection_handler = std::function<void(sock_ptr)>;

class tcp_server
{
public:
    tcp_server();
    virtual ~tcp_server();

    bool bind(unsigned short port);
    bool is_running() const;
    virtual void start();
    virtual void stop();
    void bind_new_connection_handler(const new_connection_handler &handler);
    boost::asio::io_service &io_service() const;

private:
    void accept();
    void accept_handler(const boost::system::error_code &ec, sock_ptr sock);

private:
    struct tcp_server_impl;
    std::shared_ptr<tcp_server_impl> impl_;
};