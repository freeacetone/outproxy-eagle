/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#include "proxy.hpp"

#include <array>

namespace eagle {

namespace {

// Pump bytes from one socket to the other until EOF/error, counting throughput.
// On completion shuts down the write side of the destination so the opposite
// pump observes EOF and the relay converges (clean half-close handling).
awaitable<void> pump(tcp::socket& from, tcp::socket& to, uint64_t& counter)
{
    std::array<char, 32 * 1024> buf;
    for (;;)
    {
        auto [rec, n] = co_await from.async_read_some(asio::buffer(buf),
                                                      asio::as_tuple(use_awaitable));
        if (rec || n == 0) break;
        auto [wec, w] = co_await asio::async_write(to, asio::buffer(buf, n),
                                                   asio::as_tuple(use_awaitable));
        if (wec) break;
        counter += n;
    }
    asio::error_code ec;
    to.shutdown(tcp::socket::shutdown_send, ec);
}

} // namespace

awaitable<void> relay(tcp::socket client, tcp::socket upstream,
                      uint64_t& up_bytes, uint64_t& down_bytes)
{
    using namespace asio::experimental::awaitable_operators;
    co_await (pump(client, upstream, up_bytes) && pump(upstream, client, down_bytes));

    asio::error_code ec;
    client.close(ec);
    upstream.close(ec);
}

} // namespace eagle
