/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#pragma once

#include "common.hpp"
#include "config.hpp"

namespace eagle {

class Router;
class Stats;

// Establish a connection to target host:port through the given parent (or
// directly). The hostname is passed verbatim to SOCKS5/HTTP parents and never
// resolved locally for them ("fakeresolve"); direct connections use system DNS.
// Throws on failure.
awaitable<tcp::socket> connect_upstream(const Parent& parent, std::string host, uint16_t port);

// Bidirectionally relay until either side closes. Counts bytes client->upstream
// into up_bytes and upstream->client into down_bytes.
awaitable<void> relay(tcp::socket client, tcp::socket upstream,
                      uint64_t& up_bytes, uint64_t& down_bytes);

awaitable<void> socks5_listener(tcp::endpoint ep, Router& router, Stats& stats);
awaitable<void> http_listener(tcp::endpoint ep, Router& router, Stats& stats);

} // namespace eagle
