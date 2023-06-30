// Author     : Teo.Sun
// Date       : 2022-05-25
// Description: Provide a client to connect the device to receive monitor data

#pragma once
#pragma warning(disable : 4275)

#include "cortex_sti_parser.h"
#include "tcp_client.h"
#include <atomic>
#include <functional>
#include <thread>

namespace cortex
{
    typedef std::function<void(const uint8_t *data, int len)> mon_data_callback_fun_t;
    class cortex_mon_client : public sync_tcp_client
    {
    public:
        cortex_mon_client(const std::string &ip);
        virtual ~cortex_mon_client();

        bool is_running() const
        {
            return is_running_;
        }
        void start();
        void stop();
        void add_mon_table(int compo_code);
        void set_mon_data_callback_fun(const mon_data_callback_fun_t &func);

    private:
        std::string ip_;
        std::atomic_bool is_running_ = false;
        std::vector<int> mon_tables_;
        std::vector<int> mon_request_;
        std::vector<uint8_t> recv_buf_;
        std::thread thread_;
        cortex_sti_parser_ptr sti_parser_;
        mon_data_callback_fun_t callback_func_;
    };
    typedef std::shared_ptr<cortex_mon_client> cortex_mon_client_ptr;
}  // namespace cortex