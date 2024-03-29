#include "qtexamples/VideoRecv/sti_reader.h"
#include <fmt/format.h>

int main(int argc, char **argv)
{
    boost::asio::io_context io;
    sti_reader r(io, "127.0.0.1", 3070);
    r.run(0, 0, [](auto &&msg) {
        fmt::println("==== {:02X}", fmt::join(msg, " "));
        return true;
    });
    io.run();
    // boost::asio::streambuf buffer(10240);
    // std::string tail{ (char)0xb6, (char)0x69, (char)0xfd, (char)0x2e };
    // while (true)
    //{
    //    boost::system::error_code ec;
    //    auto n = boost::asio::read_until(client, buffer, tail, ec);
    //    if (ec)
    //    {
    //        buffer.consume(buffer.max_size());
    //        continue;
    //    };
    //    std::string line((char *)buffer.data().data(), n);
    //    buffer.consume(n);

    //    fmt::println("==== {:02X}", fmt::join(line, " "));
    //}
}
