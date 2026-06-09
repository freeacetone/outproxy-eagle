/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#include "stats.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <sstream>

namespace eagle {

namespace {

std::string today_str()
{
    std::time_t t = std::time(nullptr);
    std::tm lt{};
    localtime_r(&t, &lt);
    char b[16];
    std::strftime(b, sizeof b, "%Y-%m-%d", &lt);
    return b;
}

std::string now_str()
{
    std::time_t t = std::time(nullptr);
    std::tm lt{};
    localtime_r(&t, &lt);
    char b[32];
    std::strftime(b, sizeof b, "%Y-%m-%d %H:%M:%S", &lt);
    return b;
}

// Minimal extractors for our own flat persistence JSON (no external parser).
uint64_t json_num(const std::string& s, const std::string& key)
{
    auto p = s.find("\"" + key + "\"");
    if (p == std::string::npos)
    {
        return 0;
    }
    p = s.find(':', p);
    if (p == std::string::npos)
    {
        return 0;
    }
    return std::strtoull(s.c_str() + p + 1, nullptr, 10);
}

std::string json_str(const std::string& s, const std::string& key)
{
    auto p = s.find("\"" + key + "\"");
    if (p == std::string::npos)
    {
        return {};
    }
    p = s.find(':', p);
    if (p == std::string::npos)
    {
        return {};
    }
    auto q1 = s.find('"', p);
    if (q1 == std::string::npos)
    {
        return {};
    }
    auto q2 = s.find('"', q1 + 1);
    if (q2 == std::string::npos)
    {
        return {};
    }
    return s.substr(q1 + 1, q2 - q1 - 1);
}

} // namespace

Stats::Stats(const Config& cfg)
    : m_date(today_str()),
      m_logPath(cfg.log_file),
      m_logMax(cfg.log_max_bytes)
{
    // Access logging is opt-in (privacy default). When disabled the log file is
    // never opened, so record() keeps counters only and writes nothing to disk.
    if (!cfg.logging)
    {
        return;
    }
    m_log.open(m_logPath, std::ios::app);
    if (m_log)
    {
        m_log.seekp(0, std::ios::end);
        m_logWritten = static_cast<uint64_t>(m_log.tellp());
    }
}

void Stats::roll_day_locked(const std::string& today)
{
    m_date = today;
    m_dailyUp = m_dailyDown = 0;
}

void Stats::rotate_log_locked()
{
    if (m_logMax == 0 || m_logWritten <= m_logMax)
    {
        return;
    }
    m_log.close();
    std::rename(m_logPath.c_str(), (m_logPath + ".1").c_str());
    m_log.open(m_logPath, std::ios::trunc);
    m_logWritten = 0;
}

void Stats::record(const std::string& dest, uint64_t up, uint64_t down,
                   bool blocked, const std::string& proto, const std::string& client_ip)
{
    std::lock_guard<std::mutex> lk(m_mtx);

    const std::string today = today_str();
    if (today != m_date)
    {
        roll_day_locked(today);
    }

    if (!blocked)
    {
        m_dailyUp   += up;
        m_dailyDown += down;
        m_totalUp   += up;
        m_totalDown += down;
    }

    if (m_log.is_open())
    {
        std::string line = now_str() + " " + proto + " " + client_ip + " " +
                           (blocked ? "BLOCK " : "ALLOW ") + dest +
                           " up=" + std::to_string(up) + " down=" + std::to_string(down) + "\n";
        m_log << line;
        m_log.flush();
        m_logWritten += line.size();
        rotate_log_locked();
    }
}

Stats::Snapshot Stats::snapshot() const
{
    std::lock_guard<std::mutex> lk(m_mtx);
    Snapshot s;
    s.date       = m_date;
    s.daily_up   = m_dailyUp;
    s.daily_down = m_dailyDown;
    s.total_up   = m_totalUp;
    s.total_down = m_totalDown;
    s.active     = m_active.load();
    return s;
}

void Stats::load(const std::string& path)
{
    std::ifstream f(path);
    if (!f)
    {
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string data = ss.str();

    std::lock_guard<std::mutex> lk(m_mtx);
    m_totalUp   = json_num(data, "total_upload");
    m_totalDown = json_num(data, "total_download");

    if (json_str(data, "date") == today_str())
    {
        m_date      = today_str();
        m_dailyUp   = json_num(data, "daily_upload");
        m_dailyDown = json_num(data, "daily_download");
    }
}

void Stats::dump(const std::string& path) const
{
    Snapshot s = snapshot();

    std::string data = "{\"date\":\"" + s.date + "\"" +
                       ",\"total_upload\":"   + std::to_string(s.total_up) +
                       ",\"total_download\":" + std::to_string(s.total_down) +
                       ",\"daily_upload\":"   + std::to_string(s.daily_up) +
                       ",\"daily_download\":" + std::to_string(s.daily_down) + "}\n";

    std::string tmp = path + ".tmp";
    {
        std::ofstream o(tmp, std::ios::trunc);
        if (!o)
        {
            return;
        }
        o << data;
    }
    std::rename(tmp.c_str(), path.c_str());
}

} // namespace eagle
