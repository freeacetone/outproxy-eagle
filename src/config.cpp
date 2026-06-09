/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#include "config.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace eagle {

namespace {

// Split a "host:port" / "[v6]:port" token. Returns false on parse error.
bool parse_endpoint(const std::string& token, std::string& host, uint16_t& port) {
    std::string h;
    std::string p;
    if (!token.empty() && token.front() == '[') // [::1]:8161
    {
        auto close = token.find(']');
        if (close == std::string::npos) {
            return false;
        }
        h = token.substr(1, close - 1);
        if (close + 1 >= token.size() || token[close + 1] != ':') {
            return false;
        }
        p = token.substr(close + 2);
    } else {
        auto colon = token.rfind(':'); // last colon: host:port
        if (colon == std::string::npos) {
            return false;
        }
        h = token.substr(0, colon);
        p = token.substr(colon + 1);
    }
    if (h.empty() || p.empty()) {
        return false;
    }
    try {
        port = static_cast<uint16_t>(std::stoul(p));
    } catch (...) {
        return false;
    }
    host = h;
    return true;
}

ParentType parse_parent_type(const std::string& s) {
    if (s == "socks5" || s == "socks") {
        return ParentType::Socks5;
    }
    if (s == "http") {
        return ParentType::Http;
    }
    return ParentType::Direct;
}

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) {
        out.push_back(tok);
    }
    return out;
}

} // namespace

Config Config::defaults() {
    Config c;
    c.http = {"0.0.0.0", 3128};
    c.socks = {"0.0.0.0", 1080};
    c.web = {"127.0.0.1", 8161};

    c.parents.push_back({"tor", ParentType::Socks5, "127.0.0.1", 9050});
    c.parents.push_back({"i2pd", ParentType::Http, "127.0.0.1", 4444});

    c.rules.push_back({"*.onion", false, "tor"});
    c.rules.push_back({"*.i2p", false, "i2pd"});
    c.rules.push_back({"*", false, "direct"});

    return c;
}

Config Config::load(const std::string& path) {
    Config c; // listeners disabled, no parents/rules yet; scalar defaults from struct
    std::ifstream f(path);
    if (!f) {
        std::cerr << "config: cannot open " << path << ", using built-in defaults\n";
        return Config::defaults();
    }

    bool any_listener = false;
    bool any_routing = false;
    std::string line;
    int lineno = 0;
    while (std::getline(f, line)) {
        ++lineno;
        if (auto hash = line.find('#'); hash != std::string::npos) {
            line.erase(hash);
        }
        auto t = tokenize(line);
        if (t.empty()) {
            continue;
        }
        const std::string& kw = t[0];

        auto need = [&](size_t n) {
            if (t.size() < n) {
                std::cerr << "config: line " << lineno << ": '" << kw << "' needs more arguments\n";
            }
            return t.size() >= n;
        };

        if (kw == "http" && need(2)) {
            if (parse_endpoint(t[1], c.http.host, c.http.port)) {
                any_listener = true;
            }
        } else if (kw == "socks" && need(2)) {
            if (parse_endpoint(t[1], c.socks.host, c.socks.port)) {
                any_listener = true;
            }
        } else if (kw == "web" && need(2)) {
            parse_endpoint(t[1], c.web.host, c.web.port);
        } else if (kw == "parent" && need(3)) {
            Parent p;
            p.name = t[1];
            p.type = parse_parent_type(t[2]);
            if (p.type != ParentType::Direct) {
                if (!need(4) || !parse_endpoint(t[3], p.host, p.port)) {
                    std::cerr << "config: line " << lineno << ": bad parent endpoint\n";
                    continue;
                }
            }
            c.parents.push_back(std::move(p));
        } else if (kw == "route" && need(3)) {
            c.rules.push_back({t[1], false, t[2]});
            any_routing = true;
        } else if (kw == "deny" && need(2)) {
            c.rules.push_back({t[1], true, ""});
            any_routing = true;
        } else if (kw == "set" && need(3)) {
            const std::string& key = t[1];
            const std::string& val = t[2];
            if (key == "title") {
                c.title = val;
            } else if (key == "webdir") {
                c.web_dir = val;
            } else if (key == "logfile") {
                c.log_file = val;
            } else if (key == "statsfile") {
                c.stats_file = val;
            } else if (key == "logmax") {
                c.log_max_bytes = std::stoull(val);
            } else if (key == "dumpinterval") {
                c.dump_interval = std::stoi(val);
            } else if (key == "logging") {
                c.logging = (val == "on" || val == "yes" || val == "true" || val == "1");
            } else if (key == "idletimeout") {
                c.idle_timeout = std::stoi(val);
            } else {
                std::cerr << "config: line " << lineno << ": unknown set key '" << key << "'\n";
            }
        } else {
            std::cerr << "config: line " << lineno << ": unknown directive '" << kw << "'\n";
        }
    }

    Config d = Config::defaults();
    if (!any_listener) {
        c.http = d.http;
        c.socks = d.socks;
    }
    if (!any_routing) {
        c.rules = d.rules;
    }
    if (c.parents.empty()) {
        c.parents = d.parents;
    }
    return c;
}

} // namespace eagle
