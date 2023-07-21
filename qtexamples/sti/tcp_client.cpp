#include "tcp_client.h"
#include <atomic>
#include <iostream>
#ifdef _MSC_VER
    #define _WIN32_WINNT 0x0501
#endif
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

struct sync_tcp_client::sync_tcp_client_imp_t
{
    sync_tcp_client_imp_t()
        : is_connected_(false)
        , socket_(new tcp::socket(io_service_))
    {
        socket_->open(boost::asio::ip::tcp::v4());
    }

    std::atomic_bool is_connected_;        // 连接标志
    boost::asio::io_service io_service_;   // asio对象
    std::shared_ptr<tcp::socket> socket_;  // socket对象
};

sync_tcp_client::sync_tcp_client()
    : imp_(new sync_tcp_client_imp_t())
{
}

sync_tcp_client::~sync_tcp_client()
{
    disconnect();
}

bool sync_tcp_client::bind(const std::string &ip, uint16_t port)
{
    if (imp_->is_connected_)
        return false;
    if (imp_->socket_ && imp_->socket_->is_open())
    {
        boost::system::error_code ignored_ec;
        imp_->socket_->shutdown(tcp::socket::shutdown_both, ignored_ec);
        imp_->socket_->close();
    }

    if (!imp_->socket_->is_open())
        imp_->socket_->open(boost::asio::ip::tcp::v4());
    // 构造endpoint封装ip和端口
    tcp::endpoint ep(boost::asio::ip::address::from_string(ip), port);
    boost::system::error_code ec;
    imp_->socket_->bind(ep, ec);
    if (ec)
    {
        std::cerr << ec.message() << std::endl;
        return false;
    }
    return true;
}

bool sync_tcp_client::is_connected()
{
    return imp_->is_connected_;
}

bool sync_tcp_client::connect(const std::string &ip, uint16_t port)
{
    if (!imp_->is_connected_)
    {
        try
        {
            boost::system::error_code ec;
            // 构造endpoint封装ip和端口
            tcp::endpoint ep(boost::asio::ip::address::from_string(ip), port);
            imp_->socket_->connect(ep, ec);
            if (ec)
            {
                std::cerr << ec.message() << std::endl;
                return false;
            }
            imp_->is_connected_ = true;
        }
        catch (std::exception &e)
        {
            std::cerr << e.what() << std::endl;
            return false;
        }
    }
    return true;
}

void sync_tcp_client::disconnect()
{
    if (imp_->socket_ && imp_->socket_->is_open())
    {
        boost::system::error_code ignored_ec;
        imp_->socket_->shutdown(tcp::socket::shutdown_both, ignored_ec);
        imp_->socket_->close();
    }
    if (imp_->is_connected_)
    {
        imp_->is_connected_ = false;
    }
}

int sync_tcp_client::send(const std::vector<int32_t> &buf)
{
    if (imp_->is_connected_)
    {
        try
        {
            boost::system::error_code ec;
            size_t bytes_transferred = imp_->socket_->write_some(boost::asio::buffer(buf), ec);
            if (ec)
            {
                std::cerr << ec.message() << std::endl;
                return 0;
            }
            return bytes_transferred;
        }
        catch (std::exception &e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
    return 0;
}

int sync_tcp_client::send(const std::vector<uint8_t> &buf)
{
    if (imp_->is_connected_)
    {
        try
        {
            boost::system::error_code ec;
            size_t bytes_transferred = imp_->socket_->write_some(boost::asio::buffer(buf), ec);
            if (ec)
            {
                std::cerr << ec.message() << std::endl;
                return 0;
            }
            return bytes_transferred;
        }
        catch (std::exception &e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
    return 0;
}

int sync_tcp_client::receive_some(std::vector<uint8_t> &buf)
{
    if (imp_->is_connected_)
    {
        try
        {
            boost::system::error_code ec;
            size_t bytes_transferred = imp_->socket_->read_some(boost::asio::buffer(buf), ec);
            if (ec)
            {
                std::cerr << ec.message() << std::endl;
                return 0;
            }
            return bytes_transferred;
        }
        catch (std::exception &e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
    return 0;
}

int sync_tcp_client::receive(std::vector<uint8_t> &buf, size_t size)
{
    if (imp_->is_connected_)
    {
        try
        {
            boost::system::error_code ec;
            size_t bytes_transferred = boost::asio::read(*imp_->socket_, boost::asio::buffer(buf, size), ec);
            if (ec)
            {
                std::cerr << ec.message() << std::endl;
                return 0;
            }
            return bytes_transferred;
        }
        catch (std::exception &e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
    return 0;
}
