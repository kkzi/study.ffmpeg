#pragma once

#include <boost/asio.hpp>
#include <vector>

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
namespace this_coro = boost::asio::this_coro;

///  fix it
using Iterator = boost::asio::buffers_iterator<boost::asio::streambuf::const_buffers_type>;
static std::pair<Iterator, bool> match_sti_frame(Iterator begin, Iterator end)
{
    static constexpr std::array<uint8_t, 4> HEAD{ 0x49, 0x96, 0x02, 0xd2 };
    static constexpr std::array<uint8_t, 4> TAIL{ 0xb6, 0x69, 0xfd, 0x2e };
    auto bytes = std::distance(begin, end);
    if (bytes < 68)
    {
        return { begin, false };
    }

    Iterator head_pos = end;
    bool tail = false;
    for (auto i = begin; i != end; i++)
    {
        auto cursor = boost::asio::detail::socket_ops::host_to_network_long(*(int *)(i.operator->()));
        if (cursor != 1234567890)
        {
            continue;
        }
        return { begin, true };
    }
    return { begin, false };
};

class sti_reader
{
public:
    sti_reader(boost::asio::io_context &io, std::string_view ip, uint16_t port)
        : io_(io)
        , client_(io)
        , remote_({ boost::asio::ip::address::from_string(ip.data()), port })
    {
    }

public:
    void run(int channel, int flow, std::function<bool(std::string_view msg)> on_read)
    {
        co_spawn(io_, read_sti_frame(channel, flow, std::move(on_read)), detached);
    }

private:
    static std::string make_tm_request(int channel, int flow)
    {
        std::vector<int> ints{ 1234567890, 64, 0, channel, 0, flow, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1234567890 };
        std::transform(ints.begin(), ints.end(), ints.begin(), boost::asio::detail::socket_ops::host_to_network_long);
        return std::string((char *)ints.data(), ints.size() * sizeof(int));
    }

    awaitable<void> read_sti_frame(int channel, int flow, std::function<bool(std::string_view msg)> on_read)
    {
        co_await client_.async_connect(remote_, use_awaitable);
        co_await client_.async_send(boost::asio::buffer(make_tm_request(channel, flow)), use_awaitable);
        static std::string TAIL{ (char)0xb6, (char)0x69, (char)0xfd, (char)0x2e };
        boost::asio::streambuf buffer{ 10240 };

        bool next = true;
        while (next)
        {
            boost::system::error_code ec;
            size_t n = co_await boost::asio::async_read_until(client_, buffer, TAIL, use_awaitable);
            assert(n > 0);
            if (ec)
            {
                next = on_read("");
                buffer.consume(buffer.max_size());
                continue;
            }
            std::string line((char *)buffer.data().data(), n);
            buffer.consume(n);
            next = on_read(line);
        }

        co_return;
    }

private:
    boost::asio::io_context &io_;
    boost::asio::ip::tcp::socket client_;
    boost::asio::ip::tcp::endpoint remote_;
};
