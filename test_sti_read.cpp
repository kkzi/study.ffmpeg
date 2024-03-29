#include <boost/asio.hpp>
#include <fmt/format.h>
#include <vector>

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
    for (; begin != end; begin++)
    {
        auto cursor = boost::asio::detail::socket_ops::host_to_network_long(*(int *)(begin.operator->()));
        if (cursor == 1234567890)
        {
            // head_pos = begin;
            // begin += 64;
            // continue;
            return { begin, true };
        }
        // if (cursor == -1234567890)
        //{
        //    tail = true;
        //    break;
        //}
    }
    // if (head_pos == end)
    //{
    //    return { end - 3, false };
    //}
    return { begin, false };
};

std::vector<int> make_tm_request(int channel, int data_flow)
{
    std::vector<int> ints{ 1234567890, 64, 0, channel, 0, data_flow, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1234567890 };
    std::transform(ints.begin(), ints.end(), ints.begin(), boost::asio::detail::socket_ops::host_to_network_long);
    return ints;
}

int main(int argc, char **argv)
{
    boost::asio::io_context io;
    boost::asio::ip::tcp::socket client(io);
    client.connect({ boost::asio::ip::address::from_string("127.0.0.1"), 3070 });
    bool ok = client.is_open();

    auto ints = make_tm_request(0, 0);
    std::string request((char *)ints.data(), ints.size() * sizeof(int));
    client.send(boost::asio::buffer(request));

    std::string head{ 0x49, (char)0x96, 0x02, (char)0xd2 };

    while (true)
    {
        boost::asio::streambuf buffer(568);
        // auto n = boost::asio::read_until(client, buffer, match_sti_frame);

        auto n = boost::asio::read_until(client, buffer, head);
        std::vector<uint8_t> line(n, 0);
        std::istream ss(&buffer);
        ss.read((char *)line.data(), n);
        // buffer.consume(n);
        fmt::println("==== {:02X}", fmt::join(line, " "));
    }
}
