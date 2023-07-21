#include "tm_server.h"
#include "tm_thread.h"

struct tm_server::tm_server_impl
{
    std::mutex mutex_;
    std::map<int, channel> channels_;
    std::vector<sock_ptr> socks_;                //所有连接的socket
    std::map<sock_ptr, data_ptr> readBufs_;      //所有连接的socket读取数据缓存
    std::map<sock_ptr, tm_thread_ptr> threads_;  //请求格式正确的socket和创建的线程
};

tm_server::tm_server()
    : impl_(new tm_server_impl)
{
    tcp_server::bind_new_connection_handler([&](sock_ptr socket) {
        // emit clientConnected(socket);
        impl_->socks_.push_back(socket);
        impl_->readBufs_[socket] = std::make_shared<std::vector<uint8_t>>(1024, 0x0);
        check_connection(socket);
    });
}

void tm_server::register_channel(const channel &ch)
{
    impl_->channels_[ch.id] = ch;
}

void tm_server::stop()
{
    tcp_server::stop();
    //停止时主动释放资源
    for (auto &pair : impl_->threads_)
    {
        pair.second->stop();
    }
    for (auto &socket : impl_->socks_)
    {
        if (socket->is_open())
        {
            socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
            socket->close();
        }
    }
    impl_->socks_.clear();
    impl_->readBufs_.clear();
    impl_->threads_.clear();
}

void tm_server::push(int channel, uint64_t time, data_ptr frame)
{
    impl_->mutex_.lock();
    for (auto &pair : impl_->threads_)
    {
        auto thread = pair.second;
        if (thread->channel().id == channel)
        {
            thread->push(time, frame);
        }
    }
    impl_->mutex_.unlock();
}

void tm_server::check_connection(sock_ptr socket)
{
    socket->async_read_some(boost::asio::buffer(*impl_->readBufs_[socket]),
        boost::bind(&tm_server::read_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, socket));
}

void tm_server::read_handler(const boost::system::error_code &ec, size_t bytes_transferred, sock_ptr socket)
{
    if (!tcp_server::is_running())
        return;
    if (ec)
    {  //客户端主动断开连接
        // socket->remote_endpoint().address().to_string()
        // socket->remote_endpoint().port()

        impl_->mutex_.lock();
        //析构该客户端对应的线程
        auto iter = impl_->threads_.find(socket);
        if (iter != impl_->threads_.end())
        {
            impl_->threads_[socket]->stop();
            try
            {
                if (socket->is_open())
                {
                    socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                    socket->close();
                }
            }
            catch (const std::exception &e)
            {
                printf("%s\n", e.what());
            }
            impl_->threads_.erase(iter);
        }
        //更新客户端列表
        impl_->socks_.erase(std::find(impl_->socks_.begin(), impl_->socks_.end(), socket));
        impl_->readBufs_.erase(impl_->readBufs_.find(socket));

        impl_->mutex_.unlock();
    }
    else
    {  //遥测请求
        auto ptr = (int *)impl_->readBufs_[socket]->data();
        auto channel = SwapEndian32(ptr[3]);

        if (128 == bytes_transferred && SwapEndian32(ptr[0]) == 1234567890 && SwapEndian32(ptr[1]) == bytes_transferred && SwapEndian32(ptr[31]) == -1234567890)
        {  // HDR

            if (impl_->channels_.find(channel) != impl_->channels_.end())
            {
                impl_->mutex_.lock();
                //判断遥测线程是否已经存在
                if (impl_->threads_.find(socket) == impl_->threads_.end())
                {
                    impl_->channels_[channel].block_num = SwapEndian32(ptr[8]);
                    tm_thread_ptr thread(new tm_thread(tm_thread::protocol_type::HDR, impl_->channels_[channel], socket));
                    thread->start();
                    impl_->threads_[socket] = thread;
                }
                impl_->mutex_.unlock();
            }
        }
        else if (64 == bytes_transferred && SwapEndian32(ptr[0]) == 1234567890 && SwapEndian32(ptr[1]) == bytes_transferred &&
                 SwapEndian32(ptr[15]) == -1234567890)
        {  // CRT&&RTR

            if (impl_->channels_.find(channel) != impl_->channels_.end())
            {
                impl_->mutex_.lock();
                if (impl_->threads_.find(socket) == impl_->threads_.end())
                {
                    impl_->channels_[channel].block_num = 1;
                    tm_thread_ptr thread(new tm_thread(tm_thread::protocol_type::CRT, impl_->channels_[channel], socket));
                    thread->start();
                    impl_->threads_[socket] = thread;
                }
                impl_->mutex_.unlock();
            }
        }
        else
        {
            std::vector<int> negativeReplyMsg(5);
            negativeReplyMsg[0] = SwapEndian32(1234567890);
            negativeReplyMsg[1] = SwapEndian32(20);
            negativeReplyMsg[2] = SwapEndian32(ptr[2]);
            negativeReplyMsg[3] = SwapEndian32(1);  // Bad syntax
            negativeReplyMsg[4] = SwapEndian32(-1234567890);
            //遥测请求错误响应
            boost::system::error_code ec;
            socket->write_some(boost::asio::buffer(negativeReplyMsg), ec);
        }
        check_connection(socket);
    }
}
