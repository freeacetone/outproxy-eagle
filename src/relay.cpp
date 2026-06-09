/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#include "proxy.hpp"

#include <array>
#include <atomic>
#include <chrono>

namespace eagle {

namespace {

using steady_clock = std::chrono::steady_clock;

// Pump bytes from one socket to the other until EOF/error, counting throughput.
// On completion shuts down the write side of the destination so the opposite
// pump observes EOF and the relay converges (clean half-close handling). On
// every transfer it pushes the shared idle deadline forward (when enabled).
awaitable<void> pump(tcp::socket& from, tcp::socket& to, uint64_t& counter,
                     std::atomic<int64_t>* deadline, int idle_seconds)
{
    std::array<char, 32 * 1024> buf;
    for (;;)
    {
        auto [rec, n] = co_await from.async_read_some(asio::buffer(buf),
                                                      asio::as_tuple(use_awaitable));
        if (rec || n == 0)
        {
            break;
        }
        if (deadline)
        {
            deadline->store((steady_clock::now() + std::chrono::seconds(idle_seconds))
                                .time_since_epoch()
                                .count(),
                            std::memory_order_relaxed);
        }
        auto [wec, w] = co_await asio::async_write(to, asio::buffer(buf, n),
                                                   asio::as_tuple(use_awaitable));
        if (wec)
        {
            break;
        }
        counter += n;
    }
    asio::error_code ec;
    to.shutdown(tcp::socket::shutdown_send, ec);
}

// Idle watchdog: closes both sockets once the shared deadline elapses without
// being pushed forward by a pump. It re-arms to the latest deadline each wakeup,
// so activity never has to cancel a pending timer (no race with the pumps).
awaitable<void> watchdog(tcp::socket& a, tcp::socket& b, std::atomic<int64_t>& deadline)
{
    asio::steady_timer timer(co_await asio::this_coro::executor);
    for (;;)
    {
        steady_clock::time_point dl{
            steady_clock::duration{deadline.load(std::memory_order_relaxed)}};
        if (steady_clock::now() >= dl)
        {
            asio::error_code ec;
            a.close(ec);
            b.close(ec);
            co_return;
        }
        timer.expires_at(dl);
        co_await timer.async_wait(use_awaitable); // cancelled when the relay finishes
    }
}

} // namespace

awaitable<void> relay(tcp::socket client, tcp::socket upstream,
                      uint64_t& up_bytes, uint64_t& down_bytes, int idle_seconds)
{
    using namespace asio::experimental::awaitable_operators;

    if (idle_seconds > 0)
    {
        std::atomic<int64_t> deadline{
            (steady_clock::now() + std::chrono::seconds(idle_seconds)).time_since_epoch().count()};
        co_await ((pump(client, upstream, up_bytes, &deadline, idle_seconds) &&
                   pump(upstream, client, down_bytes, &deadline, idle_seconds)) ||
                  watchdog(client, upstream, deadline));
    }
    else
    {
        co_await (pump(client, upstream, up_bytes, nullptr, 0) &&
                  pump(upstream, client, down_bytes, nullptr, 0));
    }

    asio::error_code ec;
    client.close(ec);
    upstream.close(ec);
}

} // namespace eagle
