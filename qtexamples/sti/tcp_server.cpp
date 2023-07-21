#include "tcp_server.h"
#include <iostream>

struct tcp_server::tcp_server_impl
{
    tcp_server_impl()
        : work_(new boost::asio::io_service::work(io_service_))
    {
        run_thread_ = std::make_shared<boost::thread>(boost::bind(&boost::asio::io_service::run, &io_service_));
    }

    boost::asio::io_service io_service_;                   // asio����
    std::shared_ptr<boost::asio::io_service::work> work_;  // ��֤run()û����Ϣ����ʱ���˳�
    std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_ = nullptr;
    thread_ptr run_thread_;  // run()���������̣߳������������߳�
    thread_ptr listen_thread_ = nullptr;

    uint16_t port_;
    bool is_running_ = false;
    bool is_bind_successed_ = false;
    new_connection_handler new_connection_ = nullptr;
};

tcp_server::tcp_server()
    : impl_(new tcp_server_impl)
{
}

tcp_server::~tcp_server()
{
    impl_->work_.reset();  // run()����ִ����event�˳�
    impl_->run_thread_->interrupt();
    impl_->run_thread_->join();
    impl_->run_thread_.reset();

    // run�˳���io_service���Զ�ֹͣ
    if (!impl_->io_service_.stopped())
    {
        impl_->io_service_.stop();
        impl_->io_service_.reset();
    }
}

bool tcp_server::bind(unsigned short port)
{
    try
    {
        impl_->port_ = port;
        boost::system::error_code ec;
        // ����endpoint��װip�Ͷ˿�
        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::tcp::v4(), port);
        impl_->acceptor_ = std::make_shared<boost::asio::ip::tcp::acceptor>(impl_->io_service_);  //�ظ���ʱ��Ҫ�ͷŵ�ǰacceptor
        impl_->acceptor_->open(ep.protocol());
        impl_->acceptor_->bind(ep, ec);
        if (ec)
        {
            std::cerr << ec.message() << std::endl;
            impl_->is_bind_successed_ = false;
            return false;
        }
        impl_->is_bind_successed_ = true;
        return true;
    }
    catch (std::exception e)
    {
        return false;
    }
}

bool tcp_server::is_running() const
{
    return impl_->is_running_;
}

void tcp_server::start()
{
    if (impl_->is_bind_successed_ && !impl_->is_running_)
    {
        impl_->is_running_ = true;
        impl_->listen_thread_ = std::make_shared<boost::thread>([=]() {
            if (!impl_->acceptor_->is_open())
            {  //�ر�acceptor_��û�����µ���bind�ӿ���Ҫ�ٴΰ󶨴�acceptor_
                boost::asio::ip::tcp::endpoint ep(boost::asio::ip::tcp::v4(), impl_->port_);
                impl_->acceptor_ = std::make_shared<boost::asio::ip::tcp::acceptor>(impl_->io_service_);
                impl_->acceptor_->open(ep.protocol());
                impl_->acceptor_->bind(ep);
            }
            impl_->acceptor_->listen();
            accept();
        });
    }
}

void tcp_server::stop()
{
    if (impl_->is_running_)
    {
        impl_->is_running_ = false;
        impl_->acceptor_->cancel();
        impl_->acceptor_->close();
        impl_->listen_thread_->interrupt();
        impl_->listen_thread_->join();
        impl_->listen_thread_.reset();
    }
}

void tcp_server::bind_new_connection_handler(const new_connection_handler &handler)
{
    impl_->new_connection_ = handler;
}

boost::asio::io_service &tcp_server::io_service() const
{
    return impl_->io_service_;
}

void tcp_server::accept()
{
    sock_ptr sock(new boost::asio::ip::tcp::socket(impl_->io_service_));
    impl_->acceptor_->async_accept(*sock, boost::bind(&tcp_server::accept_handler, this, boost::asio::placeholders::error, sock));
}

void tcp_server::accept_handler(const boost::system::error_code &ec, sock_ptr sock)
{
    if (!ec)
    {
        impl_->new_connection_(sock);
        accept();
    }
}