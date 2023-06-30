#include "cortex_tm_client.h"
#ifdef _MSC_VER
    #define _WIN32_WINNT 0x0501
#endif
#include <boost/asio.hpp>
#include <boost/assign.hpp>

using namespace boost::assign;

#define BUFFER_MAX_SIZE 15 * 1024 * 1024  // 1.2Gbps 最大缓存设置为100ms数据大小

namespace cortex
{
    cortex_tm_client::cortex_tm_client()
        : recv_buf_(BUFFER_MAX_SIZE)
    {
    }

    cortex_tm_client::~cortex_tm_client()
    {
        stop();
    }

    void cortex_tm_client::set_config(const cortex_tm_config &config)
    {
        assert(config.block_num >= 0 && config.block_num <= 131072);
        tm_config_ = config;
    }

    cortex::cortex_tm_config cortex_tm_client::get_config() const
    {
        return tm_config_;
    }

    void cortex_tm_client::start()
    {
        if (!is_running_)
        {
            is_running_ = true;
            thread_ = std::thread([=]() {
                while (is_running_)
                {
                    if (connect(tm_config_.ip, tm_config_.port))
                    {
                        std::vector<int32_t> start_command = tm_request();
                        std::transform(start_command.begin(), start_command.end(), start_command.begin(), std::function<int32_t(int32_t)>(htonl));

                        // 发送遥测数据请求
                        if (sync_tcp_client::send(start_command) == start_command.size() * sizeof(int32_t))
                        {
                            while (is_running_)
                            {
                                try
                                {
                                    size_t receive_bytes = sync_tcp_client::receive_some(recv_buf_);  //根据实际速率自动适应去接收
                                    if (receive_bytes > 0)
                                    {
                                        if (tm_data_callback_fun_)
                                            tm_data_callback_fun_(recv_buf_.begin(), recv_buf_.begin() + receive_bytes);
                                    }
                                    else
                                    {
                                        if (is_running_)
                                        {
                                            sync_tcp_client::disconnect();
                                            if (error_log_callback_fun_)
                                                error_log_callback_fun_("please switch the device to acquisition mode and try again.");
                                        }
                                        break;
                                    }
                                }
                                catch (std::exception e)
                                {
                                    if (error_log_callback_fun_)
                                        error_log_callback_fun_(e.what());
                                }
                            }
                        }
                        else
                        {
                            if (error_log_callback_fun_)
                                error_log_callback_fun_(tm_config_.ip + " telemetry request send failed.");
                        }
                    }
                    else
                    {
                        if (error_log_callback_fun_)
                            error_log_callback_fun_(tm_config_.ip + " telemetry port connection failed.");
                    }
                }
            });
        }
    }

    void cortex_tm_client::stop()
    {
        if (is_running_)
        {
            is_running_ = false;
            sync_tcp_client::disconnect();
            if (thread_.joinable())
            {
                thread_.join();
            }
        }
    }

    void cortex_tm_client::set_tm_data_callback_fun(const tm_data_callback_fun_t &fun)
    {
        tm_data_callback_fun_ = fun;
    }

    void cortex_tm_client::set_error_log_callback_fun(const error_log_callback_fun_t &fun)
    {
        error_log_callback_fun_ = fun;
    }

    /***************************************************************
     * @class hdr_tm_client
     * @brief HDR
     ***************************************************************/
    std::vector<int32_t> hdr_tm_client::tm_request() const
    {
        //初始化遥测数据请求
        std::vector<int32_t> start_command;
        start_command += 1234567890, 128, 0, tm_config_.channel, tm_config_.data_flow, 0, 0, 0, tm_config_.block_num, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, -1234567890;
        return start_command;
    }
    /***************************************************************
     * @class crt_tm_client
     * @brief CRT
     ***************************************************************/
    std::vector<int32_t> crt_tm_client::tm_request() const
    {
        //初始化遥测数据请求
        std::vector<int32_t> start_command;
        start_command += 1234567890, 64, 0, tm_config_.channel, 0, tm_config_.data_flow, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1234567890;
        return start_command;
    }
}  // namespace cortex
