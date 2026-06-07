/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#include "router.hpp"

#include <asio/ip/address.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>

namespace eagle {

namespace {

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Parse "addr/prefix" into 16 bytes (v4 left-aligned in 4, v6 in 16) + prefix.
bool parse_cidr(const std::string& s, std::array<uint8_t, 16>& net, int& prefix, bool& v6)
{
    auto slash = s.find('/');
    if (slash == std::string::npos)
    {
        return false;
    }
    std::string addr = s.substr(0, slash);
    try
    {
        prefix = std::stoi(s.substr(slash + 1));
    }
    catch (...)
    {
        return false;
    }

    asio::error_code ec;
    auto a = asio::ip::make_address(addr, ec);
    if (ec)
    {
        return false;
    }

    net.fill(0);
    if (a.is_v4())
    {
        v6 = false;
        auto b = a.to_v4().to_bytes();
        std::copy(b.begin(), b.end(), net.begin());
        if (prefix < 0 || prefix > 32)
        {
            return false;
        }
    }
    else
    {
        v6 = true;
        auto b = a.to_v6().to_bytes();
        std::copy(b.begin(), b.end(), net.begin());
        if (prefix < 0 || prefix > 128)
        {
            return false;
        }
    }
    return true;
}

bool cidr_match(const std::array<uint8_t, 16>& net, int prefix, bool v6, const std::string& host)
{
    asio::error_code ec;
    auto a = asio::ip::make_address(host, ec);
    if (ec)
    {
        return false; // host is not an IP literal
    }
    if (a.is_v6() != v6)
    {
        return false;
    }

    std::array<uint8_t, 16> hb{};
    hb.fill(0);
    if (v6)
    {
        auto b = a.to_v6().to_bytes();
        std::copy(b.begin(), b.end(), hb.begin());
    }
    else
    {
        auto b = a.to_v4().to_bytes();
        std::copy(b.begin(), b.end(), hb.begin());
    }

    int full = prefix / 8;
    int rem  = prefix % 8;
    for (int i = 0; i < full; ++i)
    {
        if (hb[i] != net[i])
        {
            return false;
        }
    }
    if (rem)
    {
        uint8_t mask = static_cast<uint8_t>(0xFF << (8 - rem));
        if ((hb[full] & mask) != (net[full] & mask))
        {
            return false;
        }
    }
    return true;
}

} // namespace

Router::Router(const Config& cfg)
{
    for (const auto& r : cfg.rules)
    {
        CompiledRule cr;
        cr.deny = r.deny;

        if (!r.deny)
        {
            if (r.parent == "direct" || r.parent.empty())
            {
                cr.parent = Parent{"direct", ParentType::Direct, "", 0};
            }
            else
            {
                auto it = std::find_if(cfg.parents.begin(), cfg.parents.end(),
                                       [&](const Parent& p) { return p.name == r.parent; });
                if (it == cfg.parents.end())
                {
                    std::cerr << "router: rule '" << r.pattern << "' references unknown parent '"
                              << r.parent << "', skipping\n";
                    continue;
                }
                cr.parent = *it;
            }
        }

        const std::string& pat = r.pattern;
        if (pat == "*")
        {
            cr.kind = Kind::Any;
        }
        else if (pat.rfind("*.", 0) == 0)
        {
            cr.kind = Kind::Suffix;
            cr.text = lower(pat.substr(2));
        }
        else if (!pat.empty() && pat.front() == '.')
        {
            cr.kind = Kind::Suffix;
            cr.text = lower(pat.substr(1));
        }
        else if (pat.find('/') != std::string::npos &&
                 parse_cidr(pat, cr.net, cr.prefix, cr.v6))
        {
            cr.kind = Kind::Cidr;
        }
        else
        {
            cr.kind = Kind::Exact;
            cr.text = lower(pat);
        }

        m_rules.push_back(std::move(cr));
    }
}

Router::Decision Router::decide(const std::string& rawHost) const
{
    const std::string host = lower(rawHost);
    for (const auto& r : m_rules)
    {
        bool hit = false;
        switch (r.kind)
        {
        case Kind::Any:
        {
            hit = true;
            break;
        }
        case Kind::Exact:
        {
            hit = (host == r.text);
            break;
        }
        case Kind::Suffix:
        {
            hit = (host == r.text) ||
                  (host.size() > r.text.size() + 1 &&
                   host.compare(host.size() - r.text.size() - 1, r.text.size() + 1,
                                "." + r.text) == 0);
            break;
        }
        case Kind::Cidr:
        {
            hit = cidr_match(r.net, r.prefix, r.v6, rawHost);
            break;
        }
        }

        if (hit)
        {
            return Decision{r.deny, r.parent};
        }
    }
    return Decision{true, {}}; // no match -> deny
}

} // namespace eagle
