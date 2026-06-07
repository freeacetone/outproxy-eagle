/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#include "stats.hpp"

#include "crow_all.h"

#include <algorithm>
#include <cstdio>
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

std::vector<std::pair<std::string, uint64_t>>
top_of(const std::map<std::string, uint64_t>& m, std::size_t n)
{
    std::vector<std::pair<std::string, uint64_t>> v(m.begin(), m.end());
    std::sort(v.begin(), v.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    if (v.size() > n) v.resize(n);
    return v;
}

} // namespace

Stats::Stats(const Config& cfg)
    : m_title(cfg.title),
      m_topN(cfg.top_list_size),
      m_date(today_str()),
      m_logPath(cfg.log_file),
      m_logMax(cfg.log_max_bytes)
{
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
    m_dailyTop.clear();
}

void Stats::rotate_log_locked()
{
    if (m_logMax == 0 || m_logWritten <= m_logMax) return;
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
    if (today != m_date) roll_day_locked(today);

    if (!blocked)
    {
        m_dailyUp   += up;
        m_dailyDown += down;
        m_totalUp   += up;
        m_totalDown += down;
        ++m_dailyTop[dest];
        ++m_totalTop[dest];

        if (auto it = std::find(m_last.begin(), m_last.end(), dest); it != m_last.end())
            m_last.erase(it);
        m_last.push_front(dest);
        while (m_last.size() > m_topN) m_last.pop_back();
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
    s.last.assign(m_last.begin(), m_last.end());
    s.top_daily  = top_of(m_dailyTop, m_topN);
    s.top_total  = top_of(m_totalTop, m_topN);
    return s;
}

std::string Stats::json() const
{
    Snapshot s = snapshot();

    auto top_array = [](const std::vector<std::pair<std::string, uint64_t>>& top) {
        std::vector<crow::json::wvalue> arr;
        for (const auto& [dest, count] : top)
        {
            crow::json::wvalue e;
            e["dest"]  = dest;
            e["count"] = count;
            arr.push_back(std::move(e));
        }
        return arr;
    };

    crow::json::wvalue w;
    w["title"]          = m_title;
    w["version"]        = SOFTWARE_VERSION;
    w["date"]           = s.date;
    w["daily_upload"]   = s.daily_up;
    w["daily_download"] = s.daily_down;
    w["total_upload"]   = s.total_up;
    w["total_download"] = s.total_down;
    w["last"]           = s.last;
    w["daily_top"]      = top_array(s.top_daily);
    w["total_top"]      = top_array(s.top_total);
    return w.dump();
}

void Stats::load(const std::string& path)
{
    std::ifstream f(path);
    if (!f) return;
    std::stringstream ss;
    ss << f.rdbuf();

    auto r = crow::json::load(ss.str());
    if (!r) return;

    std::lock_guard<std::mutex> lk(m_mtx);
    try
    {
        if (r.has("total_upload"))   m_totalUp   = r["total_upload"].u();
        if (r.has("total_download")) m_totalDown = r["total_download"].u();
        if (r.has("total_top"))
            for (const auto& e : r["total_top"])
                m_totalTop[std::string(e["dest"])] = e["count"].u();

        const std::string today = today_str();
        if (r.has("date") && std::string(r["date"]) == today)
        {
            m_date = today;
            if (r.has("daily_upload"))   m_dailyUp   = r["daily_upload"].u();
            if (r.has("daily_download")) m_dailyDown = r["daily_download"].u();
            if (r.has("daily_top"))
                for (const auto& e : r["daily_top"])
                    m_dailyTop[std::string(e["dest"])] = e["count"].u();
        }
    }
    catch (...)
    {
        // tolerate malformed/partial persistence files
    }
}

void Stats::dump(const std::string& path) const
{
    std::string data = json();
    std::string tmp  = path + ".tmp";
    {
        std::ofstream o(tmp, std::ios::trunc);
        if (!o) return;
        o << data;
    }
    std::rename(tmp.c_str(), path.c_str());
}

} // namespace eagle
