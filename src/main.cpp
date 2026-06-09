/*
outproxy-eagle: lightweight native HTTP/SOCKS5 proxy with web statistics.
Source: https://github.com/freeacetone/outproxy-eagle
Copyright (C) 2022-2026, acetone. GPLv3.
*/

#include "common.hpp"
#include "config.hpp"
#include "proxy.hpp"
#include "router.hpp"
#include "stats.hpp"
#include "web.hpp"

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/signal_set.hpp>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace eagle;

namespace {

void usage() {
    std::cout << SOFTWARE_NAME << " " << SOFTWARE_VERSION << "\n"
              << "Lightweight native HTTP/SOCKS5 outproxy with a JS-free web UI.\n\n"
              << "USAGE:\n"
              << "  outproxy-eagle [-c <config>]\n\n"
              << "OPTIONS:\n"
              << "  -c, --config <path>   config file (default: eagle.conf)\n"
              << "  -h, --help            show this help\n\n"
              << "Without a config file the built-in defaults are used:\n"
              << "  socks 0.0.0.0:1080, http 0.0.0.0:3128, web 127.0.0.1:8161\n"
              << "  *.onion -> tor (socks5 127.0.0.1:9050)\n"
              << "  *.i2p   -> i2pd (http  127.0.0.1:4444)\n"
              << "  *       -> direct (system DNS, no parent proxy)\n\n"
              << "Source: https://github.com/freeacetone/outproxy-eagle\n";
}

tcp::endpoint make_endpoint(const Listener& l) {
    return tcp::endpoint(asio::ip::make_address(l.host), l.port);
}

awaitable<void> dump_loop(Stats& stats, std::string path, int seconds) {
    asio::steady_timer timer(co_await asio::this_coro::executor);
    for (;;) {
        timer.expires_after(std::chrono::seconds(seconds));
        auto [ec] = co_await timer.async_wait(asio::as_tuple(use_awaitable));
        if (ec) {
            break;
        }
        stats.dump(path);
    }
}

} // namespace

int main(int argc, char** argv) {
    std::string cfg_path = "eagle.conf";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "-c" || a == "--config") && i + 1 < argc) {
            cfg_path = argv[++i];
        } else if (a == "-h" || a == "--help") {
            usage();
            return 0;
        }
    }

    Config cfg = Config::load(cfg_path);
    Stats stats(cfg);
    stats.load(cfg.stats_file);
    Router router(cfg);

    asio::io_context ioc;

    auto spawn_listener = [&](const Listener& l, const char* name, auto fn) {
        if (!l.enabled()) {
            return;
        }
        try {
            co_spawn(ioc, fn(make_endpoint(l), router, stats), asio::detached);
            std::cout << name << " proxy listening on " << l.host << ":" << l.port << "\n";
        } catch (const std::exception& e) {
            std::cerr << "failed to start " << name << " listener: " << e.what() << "\n";
        }
    };

    spawn_listener(cfg.socks, "SOCKS5", [idle = cfg.idle_timeout](auto ep, Router& r, Stats& s) {
        return socks5_listener(std::move(ep), r, s, idle);
    });
    spawn_listener(cfg.http, "HTTP", [idle = cfg.idle_timeout](auto ep, Router& r, Stats& s) {
        return http_listener(std::move(ep), r, s, idle);
    });

    co_spawn(ioc, dump_loop(stats, cfg.stats_file, cfg.dump_interval), asio::detached);

    web::start(cfg, stats);
    std::cout << "web UI on http://" << cfg.web.host << ":" << cfg.web.port << "\n";

    asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](const asio::error_code&, int) {
        std::cout << "\nshutting down...\n";
        ioc.stop();
    });

    unsigned n = std::max(2u, std::thread::hardware_concurrency());
    std::vector<std::thread> pool;
    pool.reserve(n - 1);
    for (unsigned i = 1; i < n; ++i) {
        pool.emplace_back([&ioc] { ioc.run(); });
    }
    ioc.run();
    for (auto& t : pool) {
        t.join();
    }

    web::stop();
    stats.dump(cfg.stats_file);
    std::cout << "terminated.\n";
    return 0;
}
