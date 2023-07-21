// Author     : Teo.Sun
// Date       : 2019-09-27
// Description: Provide a client to connect the device to receive telemetry data

#pragma once
#pragma warning(disable : 4275)
#pragma warning(disable : 4834)

#include "tcp_client.h"
#include <atomic>
#include <functional>
#include <thread>

namespace cortex
{
    /***************************************************************
     * @class cortex_tm_client
     * @brief cortex telemetry client base class
     ***************************************************************/
    struct cortex_tm_config
    {
        std::string ip;
        uint16_t port;
        int channel;
        int block_num;
        int data_flow;
    };

    class cortex_tm_client;
    typedef std::shared_ptr<cortex_tm_client> cortex_tm_client_ptr;

    typedef std::shared_ptr<std::vector<uint8_t>> data_ptr;
    typedef std::vector<uint8_t>::iterator iterator_type;

    typedef std::function<void(iterator_type begin, iterator_type end)> tm_data_callback_fun_t;
    typedef std::function<void(const std::string &msg)> error_log_callback_fun_t;

    class cortex_tm_client : public sync_tcp_client
    {
    public:
        cortex_tm_client();
        virtual ~cortex_tm_client();

        void set_config(const cortex_tm_config &config);
        cortex_tm_config get_config() const;

        bool is_running() const
        {
            return is_running_;
        }
        void start();
        void stop();
        void set_tm_data_callback_fun(const tm_data_callback_fun_t &fun);
        void set_error_log_callback_fun(const error_log_callback_fun_t &fun);

    protected:
        virtual std::vector<int32_t> tm_request() const = 0;

    protected:
        cortex_tm_config tm_config_;
        std::atomic_bool is_running_ = false;
        std::vector<uint8_t> recv_buf_;
        std::thread thread_;
        tm_data_callback_fun_t tm_data_callback_fun_ = nullptr;
        error_log_callback_fun_t error_log_callback_fun_ = nullptr;
    };

    /***************************************************************
     * @class hdr_tm_client
     * @brief HDR telemetry client
     ***************************************************************/
    class hdr_tm_client;
    typedef std::shared_ptr<hdr_tm_client> hdr_tm_client_ptr;

    class hdr_tm_client : public cortex_tm_client
    {
    public:
        hdr_tm_client() = default;
        virtual ~hdr_tm_client() = default;

    protected:
        virtual std::vector<int32_t> tm_request() const override;
    };
    /***************************************************************
     * @class crt_tm_client
     * @brief CRT telemetry client
     ***************************************************************/
    class crt_tm_client;
    typedef std::shared_ptr<crt_tm_client> crt_tm_client_ptr;

    class crt_tm_client : public cortex_tm_client
    {
    public:
        crt_tm_client() = default;
        virtual ~crt_tm_client() = default;

    protected:
        virtual std::vector<int32_t> tm_request() const override;
    };
}  // namespace cortex