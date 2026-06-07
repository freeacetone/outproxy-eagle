/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#pragma once

#include "config.hpp"

#include <cstdint>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace eagle {

class Stats
{
public:
    struct Snapshot
    {
        std::string date;
        uint64_t daily_up = 0, daily_down = 0, total_up = 0, total_down = 0;
        std::vector<std::string> last;
        std::vector<std::pair<std::string, uint64_t>> top_daily;
        std::vector<std::pair<std::string, uint64_t>> top_total;
    };

    explicit Stats(const Config& cfg);

    // Record one finished (or blocked) connection.
    void record(const std::string& dest, uint64_t up, uint64_t down,
                bool blocked, const std::string& proto, const std::string& client_ip);

    Snapshot    snapshot() const;
    std::string json() const;                 // JSON dump (web + persistence)

    void load(const std::string& path);       // restore counters at startup
    void dump(const std::string& path) const; // periodic persistence

private:
    void roll_day_locked(const std::string& today);
    void rotate_log_locked();

    mutable std::mutex m_mtx;

    std::string m_title;
    std::size_t m_topN;

    std::string m_date;
    uint64_t m_dailyUp = 0, m_dailyDown = 0, m_totalUp = 0, m_totalDown = 0;
    std::map<std::string, uint64_t> m_dailyTop;
    std::map<std::string, uint64_t> m_totalTop;
    std::deque<std::string> m_last;

    std::string  m_logPath;
    std::ofstream m_log;
    uint64_t      m_logMax = 0;
    uint64_t      m_logWritten = 0;
};

} // namespace eagle
