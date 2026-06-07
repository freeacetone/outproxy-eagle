/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#pragma once

#include "config.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace eagle {

class Router
{
public:
    struct Decision
    {
        bool   deny = false;
        Parent parent;             // valid when !deny
    };

    explicit Router(const Config& cfg);

    // host is a domain name or an IP literal (never resolved here -> "fakeresolve").
    // Returns the action of the first matching rule; denies if nothing matches.
    Decision decide(const std::string& host) const;

private:
    enum class Kind { Any, Suffix, Exact, Cidr };

    struct CompiledRule
    {
        Kind        kind = Kind::Exact;
        std::string text;                  // suffix base / exact host (lowercased)
        bool        v6 = false;            // for Cidr
        std::array<uint8_t, 16> net{};     // for Cidr
        int         prefix = 0;            // for Cidr
        bool        deny = false;
        Parent      parent;
    };

    std::vector<CompiledRule> m_rules;
};

} // namespace eagle
