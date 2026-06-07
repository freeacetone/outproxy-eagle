/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#include "proxy.hpp"
#include "router.hpp"
#include "stats.hpp"

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>

#include <array>
#include <string>

namespace eagle {

namespace {

// SOCKS5 reply codes
enum : uint8_t {
    REP_OK            = 0x00,
    REP_GENERAL_FAIL  = 0x01,
    REP_NOT_ALLOWED   = 0x02,
    REP_HOST_UNREACH  = 0x04,
    REP_CMD_UNSUPP    = 0x07,
    REP_ATYP_UNSUPP   = 0x08,
};

awaitable<void> reply(tcp::socket& s, uint8_t code)
{
    const uint8_t r[10] = {0x05, code, 0x00, 0x01, 0, 0, 0, 0, 0, 0};
    co_await asio::async_write(s, asio::buffer(r, 10), asio::as_tuple(use_awaitable));
}

awaitable<void> handle(tcp::socket client, Router& router, Stats& stats)
{
    std::string client_ip;
    try
    {
        client_ip = client.remote_endpoint().address().to_string();

        // Greeting: VER, NMETHODS, METHODS[]
        uint8_t hdr[2];
        co_await asio::async_read(client, asio::buffer(hdr, 2), use_awaitable);
        if (hdr[0] != 0x05)
        {
            co_return;
        }
        std::vector<uint8_t> methods(hdr[1]);
        if (hdr[1])
        {
            co_await asio::async_read(client, asio::buffer(methods), use_awaitable);
        }

        const uint8_t sel[2] = {0x05, 0x00}; // NO-AUTH (public proxy)
        co_await asio::async_write(client, asio::buffer(sel, 2), use_awaitable);

        // Request: VER, CMD, RSV, ATYP, ADDR, PORT
        uint8_t req[4];
        co_await asio::async_read(client, asio::buffer(req, 4), use_awaitable);
        if (req[0] != 0x05)
        {
            co_return;
        }
        const uint8_t cmd  = req[1];
        const uint8_t atyp = req[3];

        std::string host;
        if (atyp == 0x01) // IPv4
        {
            uint8_t a[4];
            co_await asio::async_read(client, asio::buffer(a, 4), use_awaitable);
            host = std::to_string(a[0]) + "." + std::to_string(a[1]) + "." +
                   std::to_string(a[2]) + "." + std::to_string(a[3]);
        }
        else if (atyp == 0x03) // domain
        {
            uint8_t len = 0;
            co_await asio::async_read(client, asio::buffer(&len, 1), use_awaitable);
            std::string d(len, '\0');
            co_await asio::async_read(client, asio::buffer(d.data(), len), use_awaitable);
            host = std::move(d);
        }
        else if (atyp == 0x04) // IPv6
        {
            asio::ip::address_v6::bytes_type b;
            co_await asio::async_read(client, asio::buffer(b), use_awaitable);
            host = asio::ip::make_address_v6(b).to_string();
        }
        else
        {
            co_await reply(client, REP_ATYP_UNSUPP);
            co_return;
        }

        uint8_t pb[2];
        co_await asio::async_read(client, asio::buffer(pb, 2), use_awaitable);
        const uint16_t port = static_cast<uint16_t>((pb[0] << 8) | pb[1]);

        if (cmd != 0x01) // only CONNECT
        {
            co_await reply(client, REP_CMD_UNSUPP);
            co_return;
        }

        auto decision = router.decide(host);
        if (decision.deny)
        {
            co_await reply(client, REP_NOT_ALLOWED);
            stats.record(host, 0, 0, true, "socks5", client_ip);
            co_return;
        }

        tcp::socket upstream(co_await asio::this_coro::executor);
        bool connected = true;
        try
        {
            upstream = co_await connect_upstream(decision.parent, host, port);
        }
        catch (const std::exception&)
        {
            connected = false;
        }
        if (!connected)
        {
            co_await reply(client, REP_HOST_UNREACH);
            stats.record(host, 0, 0, true, "socks5", client_ip);
            co_return;
        }

        co_await reply(client, REP_OK);

        uint64_t up = 0, down = 0;
        {
            ActiveConnectionGuard guard(stats);
            co_await relay(std::move(client), std::move(upstream), up, down);
        }
        stats.record(host, up, down, false, "socks5", client_ip);
    }
    catch (const std::exception&)
    {
        // client gone or protocol error -> drop quietly
    }
}

} // namespace

awaitable<void> socks5_listener(tcp::endpoint ep, Router& router, Stats& stats)
{
    auto ex = co_await asio::this_coro::executor;
    tcp::acceptor acceptor(ex);
    acceptor.open(ep.protocol());
    acceptor.set_option(asio::socket_base::reuse_address(true));
    acceptor.bind(ep);
    acceptor.listen();

    for (;;)
    {
        auto [ec, sock] = co_await acceptor.async_accept(asio::as_tuple(use_awaitable));
        if (ec)
        {
            continue;
        }
        co_spawn(ex, handle(std::move(sock), router, stats), asio::detached);
    }
}

} // namespace eagle
