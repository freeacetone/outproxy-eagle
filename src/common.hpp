/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#pragma once

#include <asio.hpp>
#include <asio/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>

#include <cstdint>
#include <string>

#include "config.hpp" // SOFTWARE_NAME / SOFTWARE_VERSION

using asio::ip::tcp;
using asio::awaitable;
using asio::use_awaitable;
