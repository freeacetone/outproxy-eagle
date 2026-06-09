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
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

namespace eagle {

namespace {

std::unique_ptr<crow::SimpleApp> g_app;
std::future<void> g_server; // keep alive: run_async()'s future blocks on destruction

constexpr const char* TRAFFIC_COOKIE = "trafficvolume";

// All web assets are read from disk ONCE at start() and served from memory
// thereafter; no request ever touches the filesystem.
struct CachedFile
{
    std::string data;
    std::string content_type;
};

struct WebCache
{
    std::string index_tpl;           // index.html template (or built-in default)
    std::string information_section; // pre-wrapped information box (or empty)
    std::unordered_map<std::string, CachedFile> files; // relative path -> file
};

std::unique_ptr<WebCache> g_cache;

std::string read_file(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        return {};
    }
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

// Convert a byte count to a human-readable KiB/MiB/GiB string.
std::string human_bytes(uint64_t b)
{
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    double v = static_cast<double>(b);
    int i = 0;
    while (v >= 1024.0 && i < 5)
    {
        v /= 1024.0;
        ++i;
    }
    char buf[32];
    std::snprintf(buf, sizeof buf, i == 0 ? "%.0f %s" : "%.2f %s", v, units[i]);
    return buf;
}

void replace_all(std::string& s, const std::string& from, const std::string& to)
{
    if (from.empty())
    {
        return;
    }
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos)
    {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

// Extract a named value from a Cookie request header ("a=1; trafficvolume=42").
uint64_t cookie_value(const std::string& cookie_header, const std::string& name)
{
    std::string key = name + "=";
    auto pos = cookie_header.find(key);
    if (pos == std::string::npos)
    {
        return 0;
    }
    pos += key.size();
    auto end = cookie_header.find(';', pos);
    std::string val = cookie_header.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    try
    {
        return std::stoull(val);
    }
    catch (...)
    {
        return 0;
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
<title>{{TITLE}}</title>
<link rel="icon" type="image/svg+xml" href="/favicon.svg">
<link rel="stylesheet" href="/style.css">
</head>
<body>
<main>
  <header>
    <h1>{{TITLE}}</h1>
    <div class="active"><span class="num">{{ACTIVE}}</span> active connections</div>
  </header>

  <section class="traffic">
    <div class="card">
      <h3>Today</h3>
      <div class="row"><span>RX</span><b>{{DAILY_DOWN}}</b></div>
      <div class="row"><span>TX</span><b>{{DAILY_UP}}</b></div>
    </div>
    <div class="card">
      <h3>Total</h3>
      <div class="row"><span>RX</span><b>{{TOTAL_DOWN}}</b></div>
      <div class="row"><span>TX</span><b>{{TOTAL_UP}}</b></div>
    </div>
  </section>

  <p class="since">{{TRAFFIC_LINE}}</p>

  {{INFORMATION}}

  <footer><a href="https://github.com/freeacetone/outproxy-eagle">outproxy-eagle</a> v{{VERSION}}</footer>
</main>
</body>
</html>
)HTML";
}

// Render the page. last_visit_volume comes from the client's cookie; the
// current cumulative volume is written back to *current_volume for the cookie.
std::string render_index(const Config& cfg, Stats& stats,
                         uint64_t last_visit_volume, uint64_t* current_volume)
{
    auto snap = stats.snapshot();

    const uint64_t current = snap.total_up + snap.total_down;
    if (current_volume)
    {
        *current_volume = current;
    }
    const uint64_t since = (current >= last_visit_volume) ? (current - last_visit_volume) : current;

    std::string tpl         = g_cache->index_tpl;
    std::string information  = g_cache->information_section;

    // Show the delta since the visitor's last visit; fall back to the all-time
    // total when there is no new traffic to report (delta == 0).
    std::string traffic_line;
    if (since > 0)
    {
        traffic_line = "Traffic since your last visit: <strong>" + human_bytes(since) + "</strong>";
    }
    else
    {
        traffic_line = "Total traffic served: <strong>" + human_bytes(current) + "</strong>";
    }

    replace_all(tpl, "{{TITLE}}",         html_escape(cfg.title));
    replace_all(tpl, "{{VERSION}}",       SOFTWARE_VERSION);
    replace_all(tpl, "{{ACTIVE}}",        std::to_string(snap.active));
    replace_all(tpl, "{{DAILY_UP}}",      human_bytes(snap.daily_up));
    replace_all(tpl, "{{DAILY_DOWN}}",    human_bytes(snap.daily_down));
    replace_all(tpl, "{{TOTAL_UP}}",      human_bytes(snap.total_up));
    replace_all(tpl, "{{TOTAL_DOWN}}",    human_bytes(snap.total_down));
    replace_all(tpl, "{{TRAFFIC_LINE}}",  traffic_line);
    replace_all(tpl, "{{INFORMATION}}",   information);
    return tpl;
}

const char* content_type(const std::string& path)
{
    auto dot = path.rfind('.');
    std::string ext = (dot == std::string::npos) ? "" : path.substr(dot + 1);
    if (ext == "css")
    {
        return "text/css; charset=utf-8";
    }
    if (ext == "html")
    {
        return "text/html; charset=utf-8";
    }
    if (ext == "txt")
    {
        return "text/plain; charset=utf-8";
    }
    if (ext == "png")
    {
        return "image/png";
    }
    if (ext == "jpg" || ext == "jpeg")
    {
        return "image/jpeg";
    }
    if (ext == "gif")
    {
        return "image/gif";
    }
    if (ext == "svg")
    {
        return "image/svg+xml";
    }
    if (ext == "ico")
    {
        return "image/x-icon";
    }
    if (ext == "json")
    {
        return "application/json";
    }
    return "application/octet-stream";
}

crow::response serve_static(const std::string& rel)
{
    auto it = g_cache->files.find(rel);
    if (it == g_cache->files.end())
    {
        return crow::response(crow::status::NOT_FOUND);
    }
    crow::response r(it->second.data);
    r.set_header("Content-Type", it->second.content_type);
    return r;
}

// Read every regular file under web_dir into memory once. Called from start();
// after this no request handler touches the filesystem.
void build_cache(const Config& cfg)
{
    g_cache = std::make_unique<WebCache>();

    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path root(cfg.web_dir);
    for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file(ec))
        {
            continue;
        }
        std::string rel = fs::relative(it->path(), root, ec).generic_string();
        if (ec || rel.empty())
        {
            continue;
        }
        g_cache->files.emplace(rel, CachedFile{read_file(it->path().string()), content_type(rel)});
    }

    auto idx = g_cache->files.find("index.html");
    g_cache->index_tpl = (idx != g_cache->files.end() && !idx->second.data.empty())
                             ? idx->second.data
                             : default_template();

    auto info = g_cache->files.find("information.html");
    if (info != g_cache->files.end() && !info->second.data.empty())
    {
        g_cache->information_section = "<section class=\"information\">" + info->second.data + "</section>";
    }
}

} // namespace

void web::start(const Config& cfg, Stats& stats)
{
    build_cache(cfg);

    g_app = std::make_unique<crow::SimpleApp>();
    auto& app = *g_app;
    app.loglevel(crow::LogLevel::Warning);

    auto index = [&cfg, &stats](const crow::request& req) {
        uint64_t last = cookie_value(req.get_header_value("Cookie"), TRAFFIC_COOKIE);
        uint64_t current = 0;
        crow::response r(render_index(cfg, stats, last, &current));
        r.set_header("Content-Type", "text/html; charset=utf-8");
        r.set_header("Set-Cookie", std::string(TRAFFIC_COOKIE) + "=" + std::to_string(current) +
                                       "; Max-Age=31536000; Path=/; SameSite=Lax");
        return r;
    };
    CROW_ROUTE(app, "/")(index);
    CROW_ROUTE(app, "/index.html")(index);

    CROW_ROUTE(app, "/<path>")([](const std::string& p) {
        return serve_static(p);
    });

    app.signal_clear(); // our own SIGINT/SIGTERM handling in main()
    g_server = app.bindaddr(cfg.web.host).port(cfg.web.port).concurrency(2).run_async();
}

void web::stop()
{
    if (g_app)
    {
        g_app->stop();
        if (g_server.valid())
        {
            g_server.wait();
        }
        g_app.reset();
    }
}

} // namespace eagle
