/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eagle {

#ifndef OUTPROXY_VERSION
#define OUTPROXY_VERSION "0.0.0-dev" // overridden by the build (from the git tag)
#endif

inline constexpr const char* SOFTWARE_NAME    = "outproxy-eagle";
inline constexpr const char* SOFTWARE_VERSION = OUTPROXY_VERSION;

struct Listener
{
    std::string host;
    uint16_t    port = 0;          // 0 == disabled
    bool enabled() const { return port != 0; }
};

enum class ParentType { Direct, Socks5, Http };

struct Parent
{
    std::string name;
    ParentType  type = ParentType::Direct;
    std::string host;
    uint16_t    port = 0;
};

struct Rule
{
    std::string pattern;           // "*", "*.onion", "vk.com", "10.0.0.0/8", "200::/7"
    bool        deny = false;
    std::string parent;            // parent name (empty when deny)
};

struct Config
{
    Listener http  {"", 0};
    Listener socks {"", 0};
    Listener web   {"127.0.0.1", 8161};

    std::vector<Parent> parents;
    std::vector<Rule>   rules;

    std::string title         = "outproxy-eagle";
    std::string web_dir       = "web";
    std::string log_file      = "eagle.log";
    std::string stats_file    = "state.json"; // internal counter persistence
    uint64_t    log_max_bytes = 10ull * 1024 * 1024;
    int         dump_interval = 30; // seconds
    bool        logging       = false; // access log off by default (privacy)

    // Built-in defaults: onion -> tor (socks5), i2p -> i2pd (http), rest -> direct.
    static Config defaults();

    // Parse a config file; any section omitted falls back to defaults().
    static Config load(const std::string& path);
};

} // namespace eagle
