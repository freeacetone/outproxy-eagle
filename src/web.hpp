/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#pragma once

#include "config.hpp"

namespace eagle {

class Stats;

namespace web {

// Launch the Crow web UI in background threads. References must outlive stop().
void start(const Config& cfg, Stats& stats);
void stop();

} // namespace web
} // namespace eagle
