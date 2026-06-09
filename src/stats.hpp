/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#pragma once

#include "config.hpp"

#include <atomic>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>

namespace eagle {

class Stats {
public:
    struct Snapshot {
        std::string date;
        uint64_t daily_up = 0, daily_down = 0, total_up = 0, total_down = 0;
        int64_t active = 0;
    };

    explicit Stats(const Config& cfg);

    // Record one finished (or blocked) connection: updates counters and the
    // plain-text access log. dest/proto/client_ip are logged only.
    void record(const std::string& dest, uint64_t up, uint64_t down, bool blocked,
                const std::string& proto, const std::string& client_ip);

    // Active (currently open) proxied connections.
    void connectionOpened() {
        ++m_active;
    }
    void connectionClosed() {
        --m_active;
    }
    int64_t activeConnections() const {
        return m_active.load();
    }

    Snapshot snapshot() const;

    void load(const std::string& path);       // restore counters at startup
    void dump(const std::string& path) const; // periodic persistence (mini JSON)

private:
    void roll_day_locked(const std::string& today);
    void rotate_log_locked();

    mutable std::mutex m_mtx;

    std::string m_date;
    uint64_t m_dailyUp = 0, m_dailyDown = 0, m_totalUp = 0, m_totalDown = 0;

    std::atomic<int64_t> m_active{0};

    std::string m_logPath;
    std::ofstream m_log;
    uint64_t m_logMax = 0;
    uint64_t m_logWritten = 0;
};

// RAII: counts one active connection for its lifetime (exception-safe, lives in
// the relaying coroutine frame).
class ActiveConnectionGuard {
public:
    explicit ActiveConnectionGuard(Stats& s) : m_stats(s) {
        m_stats.connectionOpened();
    }
    ~ActiveConnectionGuard() {
        m_stats.connectionClosed();
    }
    ActiveConnectionGuard(const ActiveConnectionGuard&) = delete;
    ActiveConnectionGuard& operator=(const ActiveConnectionGuard&) = delete;

private:
    Stats& m_stats;
};

} // namespace eagle
