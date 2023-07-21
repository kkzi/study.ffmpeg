#pragma once
#pragma warning(disable : 4996)

#include "tm_server.h"
#include <boost/lockfree/spsc_queue.hpp>
#include <thread>

class tm_thread
{
public:
    enum class protocol_type
    {
        CRT,
        HDR
    };
    tm_thread(protocol_type ptype, tm_server::channel channel, sock_ptr socket);
    ~tm_thread();

public:
    void start();
    void stop();

    void push(uint64_t epoch_ms, data_ptr frame);

    tm_server::channel channel() const;

private:
    void sim_crt();
    void sim_hdr();

private:
    protocol_type ptype_;
    tm_server::channel channel_;
    sock_ptr socket_;

    std::atomic_bool is_running_{ false };
    std::atomic_uint send_count_{ 0 };
    std::atomic_uint lost_count_{ 0 };
    std::thread thread_;
    boost::lockfree::spsc_queue<data_ptr> queue_{ 600 };
};
using tm_thread_ptr = std::shared_ptr<tm_thread>;
