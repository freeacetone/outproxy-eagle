/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#include "proxy.hpp"

#include <stdexcept>
#include <vector>

namespace eagle {

namespace {

// SOCKS5 client handshake towards a parent proxy. Passes the hostname as a
// domain (ATYP=0x03) so the parent does the resolving (fakeresolve).
awaitable<void> socks5_to_parent(tcp::socket& s, const std::string& host, uint16_t port)
{
    if (host.size() > 255)
    {
        throw std::runtime_error("hostname too long for SOCKS5");
    }

    const uint8_t greet[3] = {0x05, 0x01, 0x00}; // VER, NMETHODS=1, NO-AUTH
    co_await asio::async_write(s, asio::buffer(greet, 3), use_awaitable);

    uint8_t sel[2];
    co_await asio::async_read(s, asio::buffer(sel, 2), use_awaitable);
    if (sel[0] != 0x05 || sel[1] != 0x00)
    {
        throw std::runtime_error("SOCKS5 parent rejected auth method");
    }

    std::vector<uint8_t> req;
    req.reserve(7 + host.size());
    req.push_back(0x05);                          // VER
    req.push_back(0x01);                          // CMD=CONNECT
    req.push_back(0x00);                          // RSV
    req.push_back(0x03);                          // ATYP=DOMAINNAME
    req.push_back(static_cast<uint8_t>(host.size()));
    req.insert(req.end(), host.begin(), host.end());
    req.push_back(static_cast<uint8_t>(port >> 8));
    req.push_back(static_cast<uint8_t>(port & 0xFF));
    co_await asio::async_write(s, asio::buffer(req), use_awaitable);

    uint8_t rep[4];
    co_await asio::async_read(s, asio::buffer(rep, 4), use_awaitable);
    if (rep[1] != 0x00)
    {
        throw std::runtime_error("SOCKS5 parent CONNECT failed");
    }

    std::size_t addrlen = 0;
    if (rep[3] == 0x01)
    {
        addrlen = 4;
    }
    else if (rep[3] == 0x04)
    {
        addrlen = 16;
    }
    else if (rep[3] == 0x03)
    {
        uint8_t l = 0;
        co_await asio::async_read(s, asio::buffer(&l, 1), use_awaitable);
        addrlen = l;
    }
    std::vector<uint8_t> skip(addrlen + 2); // bound addr + port
    co_await asio::async_read(s, asio::buffer(skip), use_awaitable);
}

// HTTP CONNECT handshake towards a parent proxy.
awaitable<void> http_connect_to_parent(tcp::socket& s, const std::string& host, uint16_t port)
{
    std::string hp = host + ":" + std::to_string(port);
    std::string req = "CONNECT " + hp + " HTTP/1.1\r\nHost: " + hp + "\r\n\r\n";
    co_await asio::async_write(s, asio::buffer(req), use_awaitable);

    std::string resp;
    co_await asio::async_read_until(s, asio::dynamic_buffer(resp, 16384), "\r\n\r\n", use_awaitable);
    // Status line: "HTTP/1.1 200 ..."
    auto sp = resp.find(' ');
    if (sp == std::string::npos || sp + 1 >= resp.size() || resp[sp + 1] != '2')
    {
        throw std::runtime_error("HTTP parent CONNECT failed: " + resp.substr(0, resp.find('\r')));
    }
}

} // namespace

awaitable<tcp::socket> connect_upstream(const Parent& parent, std::string host, uint16_t port)
{
    auto ex = co_await asio::this_coro::executor;
    tcp::socket sock(ex);
    tcp::resolver resolver(ex);

    if (parent.type == ParentType::Direct)
    {
        auto eps = co_await resolver.async_resolve(host, std::to_string(port), use_awaitable);
        co_await asio::async_connect(sock, eps, use_awaitable);
        co_return sock;
    }

    auto eps = co_await resolver.async_resolve(parent.host, std::to_string(parent.port), use_awaitable);
    co_await asio::async_connect(sock, eps, use_awaitable);

    if (parent.type == ParentType::Socks5)
    {
        co_await socks5_to_parent(sock, host, port);
    }
    else
    {
        co_await http_connect_to_parent(sock, host, port);
    }

    co_return sock;
}

} // namespace eagle
