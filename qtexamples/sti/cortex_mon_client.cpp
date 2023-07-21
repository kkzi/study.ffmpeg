#include "cortex_mon_client.h"
#ifdef _MSC_VER
    #define _WIN32_WINNT 0x0501
#endif
#include <boost/asio.hpp>
#include <boost/assign.hpp>

using namespace boost::assign;

cortex::cortex_mon_client::cortex_mon_client(const std::string &ip)
    : ip_(ip)
    , recv_buf_(1024, 0x00)
    , sti_parser_(new cortex_sti_parser(1'000'000))
{
    mon_request_ += 1234567890, 20, 0, 0, -1234567890;
    std::transform(mon_request_.begin(), mon_request_.end(), mon_request_.begin(), std::function<int(int)>(htonl));

    sti_parser_->set_tm_msg_callback_fun([&](const cortex::data_ptr tm_msg) {
        if (tm_msg->size() == 20)
        {
            auto ptr = (int *)recv_buf_.data();
            if (ptr[2] != mon_request_[2])
            {
                mon_request_[2] = ptr[2];
            }
        }
        else
        {
            if (callback_func_)
            {
                callback_func_(tm_msg->data() + 16, tm_msg->size() - 20);
            }
        }
    });
}

cortex::cortex_mon_client::~cortex_mon_client()
{
    stop();
}

void cortex::cortex_mon_client::start()
{
    if (!is_running_)
    {
        is_running_ = true;
        thread_ = std::thread([=]() {
            if (sync_tcp_client::connect(ip_, 3000))
            {
                sti_parser_->start();
                while (is_running_)
                {
                    //发送请求
                    for (auto &table : mon_tables_)
                    {
                        mon_request_[3] = htonl(table);
                        sync_tcp_client::send(mon_request_);
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    //接收反馈
                    size_t receive_bytes = sync_tcp_client::receive_some(recv_buf_);  //根据实际速率自动适应去接收
                    if (receive_bytes > 0)
                    {
                        sti_parser_->push_data(recv_buf_.begin(), recv_buf_.begin() + receive_bytes);
                    }
                    else
                    {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
        });
    }
}

void cortex::cortex_mon_client::stop()
{
    if (is_running_)
    {
        is_running_ = false;
        sync_tcp_client::disconnect();
        if (thread_.joinable())
        {
            thread_.join();
        }
        sti_parser_->stop();
    }
}

void cortex::cortex_mon_client::add_mon_table(int compo_code)
{
    mon_tables_.push_back(compo_code);
}

void cortex::cortex_mon_client::set_mon_data_callback_fun(const mon_data_callback_fun_t &func)
{
    callback_func_ = func;
}
