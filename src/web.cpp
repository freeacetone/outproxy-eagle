/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Copyright (C) 2022-2026, acetone. GPLv3.

The web UI is server-rendered HTML and contains ZERO JavaScript. Live values
refresh via an HTML <meta http-equiv="refresh"> tag, not scripting.
*/

#include "web.hpp"
#include "stats.hpp"

#include "crow_all.h"

#include <cstdio>
#include <fstream>
#include <future>
#include <memory>
#include <sstream>
#include <string>

namespace eagle {

namespace {

std::unique_ptr<crow::SimpleApp> g_app;
std::future<void> g_server; // keep alive: run_async()'s future blocks on destruction

std::string read_file(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string html_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        switch (c)
        {
        case '&': out += "&amp;";  break;
        case '<': out += "&lt;";   break;
        case '>': out += "&gt;";   break;
        case '"': out += "&quot;"; break;
        default:  out += c;        break;
        }
    }
    return out;
}

std::string human_bytes(uint64_t b)
{
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    double v = static_cast<double>(b);
    int i = 0;
    while (v >= 1024.0 && i < 5) { v /= 1024.0; ++i; }
    char buf[32];
    std::snprintf(buf, sizeof buf, i == 0 ? "%.0f %s" : "%.2f %s", v, units[i]);
    return buf;
}

void replace_all(std::string& s, const std::string& from, const std::string& to)
{
    if (from.empty()) return;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos)
    {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

const char* default_template()
{
    return
R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="30">
<title>{{TITLE}}</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<main>
<h1>{{TITLE}}</h1>
<section class="traffic">
  <div class="card"><h3>Today &uarr;</h3><p>{{DAILY_UP}}</p></div>
  <div class="card"><h3>Today &darr;</h3><p>{{DAILY_DOWN}}</p></div>
  <div class="card"><h3>Total &uarr;</h3><p>{{TOTAL_UP}}</p></div>
  <div class="card"><h3>Total &darr;</h3><p>{{TOTAL_DOWN}}</p></div>
</section>
{{INFORMATION}}
<section class="lists">
  <div><h2>Last destinations</h2><ul class="last">{{LAST}}</ul></div>
  <div><h2>Top today</h2><ul class="top">{{TOP_DAILY}}</ul></div>
  <div><h2>Top total</h2><ul class="top">{{TOP_TOTAL}}</ul></div>
</section>
<footer>{{TITLE}} &middot; outproxy-eagle v{{VERSION}} &middot; {{DATE}}</footer>
</main>
</body>
</html>
)HTML";
}

std::string render_index(const Config& cfg, Stats& stats)
{
    auto snap = stats.snapshot();

    std::string tpl = read_file(cfg.web_dir + "/index.html");
    if (tpl.empty()) tpl = default_template();

    std::string last;
    for (const auto& d : snap.last)
        last += "<li>" + html_escape(d) + "</li>";

    auto render_top = [](const std::vector<std::pair<std::string, uint64_t>>& top) {
        std::string out;
        for (const auto& [d, c] : top)
            out += "<li><span class=\"dest\">" + html_escape(d) +
                   "</span><span class=\"count\">" + std::to_string(c) + "</span></li>";
        return out;
    };

    std::string information = read_file(cfg.web_dir + "/information.html");
    if (!information.empty())
        information = "<section class=\"information\">" + information + "</section>";

    replace_all(tpl, "{{TITLE}}",      html_escape(cfg.title));
    replace_all(tpl, "{{VERSION}}",    SOFTWARE_VERSION);
    replace_all(tpl, "{{DATE}}",       html_escape(snap.date));
    replace_all(tpl, "{{DAILY_UP}}",   human_bytes(snap.daily_up));
    replace_all(tpl, "{{DAILY_DOWN}}", human_bytes(snap.daily_down));
    replace_all(tpl, "{{TOTAL_UP}}",   human_bytes(snap.total_up));
    replace_all(tpl, "{{TOTAL_DOWN}}", human_bytes(snap.total_down));
    replace_all(tpl, "{{LAST}}",       last);
    replace_all(tpl, "{{TOP_DAILY}}",  render_top(snap.top_daily));
    replace_all(tpl, "{{TOP_TOTAL}}",  render_top(snap.top_total));
    replace_all(tpl, "{{INFORMATION}}", information);
    return tpl;
}

const char* content_type(const std::string& path)
{
    auto dot = path.rfind('.');
    std::string ext = (dot == std::string::npos) ? "" : path.substr(dot + 1);
    if (ext == "css")  return "text/css; charset=utf-8";
    if (ext == "html") return "text/html; charset=utf-8";
    if (ext == "txt")  return "text/plain; charset=utf-8";
    if (ext == "png")  return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif")  return "image/gif";
    if (ext == "svg")  return "image/svg+xml";
    if (ext == "ico")  return "image/x-icon";
    if (ext == "json") return "application/json";
    return "application/octet-stream";
}

crow::response serve_static(const Config& cfg, const std::string& rel)
{
    if (rel.find("..") != std::string::npos)
        return crow::response(crow::status::NOT_FOUND);

    std::string data = read_file(cfg.web_dir + "/" + rel);
    if (data.empty())
    {
        std::ifstream probe(cfg.web_dir + "/" + rel, std::ios::binary);
        if (!probe) return crow::response(crow::status::NOT_FOUND);
    }
    crow::response r(data);
    r.set_header("Content-Type", content_type(rel));
    return r;
}

} // namespace

void web::start(const Config& cfg, Stats& stats)
{
    g_app = std::make_unique<crow::SimpleApp>();
    auto& app = *g_app;
    app.loglevel(crow::LogLevel::Warning);

    auto index = [&cfg, &stats] {
        crow::response r(render_index(cfg, stats));
        r.set_header("Content-Type", "text/html; charset=utf-8");
        return r;
    };
    CROW_ROUTE(app, "/")(index);
    CROW_ROUTE(app, "/index.html")(index);

    CROW_ROUTE(app, "/stats.json")([&stats] {
        crow::response r(stats.json());
        r.set_header("Content-Type", "application/json");
        return r;
    });

    CROW_ROUTE(app, "/<path>")([&cfg](const std::string& p) {
        return serve_static(cfg, p);
    });

    app.signal_clear(); // our own SIGINT/SIGTERM handling in main()
    g_server = app.bindaddr(cfg.web.host).port(cfg.web.port).concurrency(2).run_async();
}

void web::stop()
{
    if (g_app)
    {
        g_app->stop();
        if (g_server.valid()) g_server.wait();
        g_app.reset();
    }
}

} // namespace eagle
