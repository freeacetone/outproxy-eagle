/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#include "proxy.hpp"
#include "router.hpp"
#include "stats.hpp"

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>

#include <string>

namespace eagle {

namespace {

awaitable<void> send_status(tcp::socket& s, const char* status_line)
{
    std::string r = std::string("HTTP/1.1 ") + status_line + "\r\nConnection: close\r\n"
                    "Content-Length: 0\r\n\r\n";
    co_await asio::async_write(s, asio::buffer(r), asio::as_tuple(use_awaitable));
}

// Split "host:port" with a default port. IPv6 literals may be bracketed.
void split_host_port(const std::string& authority, std::string& host, uint16_t& port, uint16_t def)
{
    port = def;
    if (!authority.empty() && authority.front() == '[') // [::1]:443
    {
        auto close = authority.find(']');
        host = authority.substr(1, close - 1);
        if (close + 1 < authority.size() && authority[close + 1] == ':')
        {
            port = static_cast<uint16_t>(std::stoul(authority.substr(close + 2)));
        }
        return;
    }
    auto colon = authority.rfind(':');
    if (colon == std::string::npos)
    {
        host = authority;
        return;
    }
    host = authority.substr(0, colon);
    try
    {
        port = static_cast<uint16_t>(std::stoul(authority.substr(colon + 1)));
    }
    catch (...)
    {
    }
}

awaitable<void> handle(tcp::socket client, Router& router, Stats& stats, int idle_seconds)
{
    std::string client_ip;
    try
    {
        client_ip = client.remote_endpoint().address().to_string();

        std::string buf;
        std::size_t hdr_len = co_await asio::async_read_until(
            client, asio::dynamic_buffer(buf, 64 * 1024), "\r\n\r\n", use_awaitable);

        const std::string request_line = buf.substr(0, buf.find("\r\n"));
        const std::string leftover     = buf.substr(hdr_len); // bytes past header block

        // METHOD SP TARGET SP VERSION
        auto sp1 = request_line.find(' ');
        auto sp2 = request_line.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos)
        {
            co_await send_status(client, "400 Bad Request");
            co_return;
        }
        const std::string method  = request_line.substr(0, sp1);
        const std::string target  = request_line.substr(sp1 + 1, sp2 - sp1 - 1);
        const std::string version = request_line.substr(sp2 + 1);

        std::string host;
        uint16_t    port = 0;
        bool        is_connect = (method == "CONNECT");
        std::string path = "/";

        if (is_connect)
        {
            split_host_port(target, host, port, 443);
        }
        else
        {
            // absolute-form: http://host[:port]/path
            std::string rest = target;
            auto scheme = rest.find("://");
            if (scheme != std::string::npos)
            {
                rest = rest.substr(scheme + 3);
            }
            auto slash = rest.find('/');
            std::string authority = (slash == std::string::npos) ? rest : rest.substr(0, slash);
            path = (slash == std::string::npos) ? "/" : rest.substr(slash);
            split_host_port(authority, host, port, 80);
        }

        if (host.empty())
        {
            co_await send_status(client, "400 Bad Request");
            co_return;
        }

        auto decision = router.decide(host);
        if (decision.deny)
        {
            co_await send_status(client, "403 Forbidden");
            stats.record(host, 0, 0, true, "http", client_ip);
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
            co_await send_status(client, "502 Bad Gateway");
            stats.record(host, 0, 0, true, "http", client_ip);
            co_return;
        }

        uint64_t up = 0, down = 0;

        if (is_connect)
        {
            const char* ok = "HTTP/1.1 200 Connection Established\r\n\r\n";
            co_await asio::async_write(client, asio::buffer(ok, std::char_traits<char>::length(ok)),
                                       use_awaitable);
            if (!leftover.empty())
            {
                co_await asio::async_write(upstream, asio::buffer(leftover), use_awaitable);
                up += leftover.size();
            }
        }
        else
        {
            std::string outbound;
            if (decision.parent.type == ParentType::Http)
            {
                // Parent is an HTTP proxy: forward the request as received (absolute-form).
                outbound = buf.substr(0, hdr_len);
            }
            else
            {
                // Direct / SOCKS5 parent: rewrite request-line to origin-form.
                std::string headers_after_line = buf.substr(buf.find("\r\n") + 2, hdr_len - (buf.find("\r\n") + 2));
                outbound = method + " " + path + " " + version + "\r\n" + headers_after_line;
            }
            co_await asio::async_write(upstream, asio::buffer(outbound), use_awaitable);
            up += outbound.size();
            if (!leftover.empty())
            {
                co_await asio::async_write(upstream, asio::buffer(leftover), use_awaitable);
                up += leftover.size();
            }
        }

        {
            ActiveConnectionGuard guard(stats);
            co_await relay(std::move(client), std::move(upstream), up, down, idle_seconds);
        }
        stats.record(host, up, down, false, "http", client_ip);
    }
    catch (const std::exception&)
    {
        // client gone or protocol error -> drop quietly
    }
}

} // namespace

awaitable<void> http_listener(tcp::endpoint ep, Router& router, Stats& stats, int idle_seconds)
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
        co_spawn(ex, handle(std::move(sock), router, stats, idle_seconds), asio::detached);
    }
}

} // namespace eagle
