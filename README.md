# outproxy-eagle

A lightweight, dependency-free native HTTP + SOCKS5 **outproxy** with a
JavaScript-free web statistics page.

Source: <https://github.com/freeacetone/outproxy-eagle>

This is a from-scratch C++20 rewrite of the original Qt-based `3proxy-eagle`.
It no longer wraps an external `3proxy` process — the proxy cores are
implemented natively on top of [standalone Asio](https://think-async.com/Asio/),
and the web UI uses the header-only [Crow](https://crowcpp.org/) framework.
The result is a single self-contained binary that links only
`libc`/`libstdc++`/`pthread`.

## Features

- **HTTP proxy** (`CONNECT` tunnels + plain absolute-form forwarding)
- **SOCKS5 proxy** (CONNECT, domain/IPv4/IPv6 targets)
- **Rule-based routing to parent proxies** by domain suffix, exact host, or
  CIDR — or direct connection via system DNS
- Hostnames are passed to SOCKS5/HTTP parents verbatim (never resolved
  locally — "fakeresolve")
- **Public by design**: no authentication
- **JS-free web UI**: traffic counters, last/top destinations, an editable
  Information box (donation info, config examples, ...), and `/stats.json`
- Simple logging: plain-text access log with size rotation + periodic JSON
  stats dump that survives restarts

## Default configuration

With no config file, the built-in defaults are:

| Listener | Bind            |
|----------|-----------------|
| SOCKS5   | `0.0.0.0:1080`  |
| HTTP     | `0.0.0.0:3128`  |
| Web UI   | `127.0.0.1:8161`|

Routing:

| Pattern    | Destination                         |
|------------|-------------------------------------|
| `*.onion`  | Tor   (`socks5 127.0.0.1:9050`)     |
| `*.i2p`    | i2pd  (`http   127.0.0.1:4444`)     |
| `*`        | direct (system DNS, no parent)      |

## Build

Requirements: a C++20 compiler, CMake ≥ 3.16, and `libasio-dev`
(standalone Asio). Crow is vendored in `third_party/crow_all.h`.

```sh
sudo apt install build-essential cmake libasio-dev
cmake -B build
cmake --build build -j
```

The binary is `build/outproxy-eagle`.

## Run

```sh
./build/outproxy-eagle              # built-in defaults
./build/outproxy-eagle -c eagle.conf
```

Copy `eagle.conf.example` to `eagle.conf` to customize. See that file for the
full directive reference.

### Running as a service

A hardened systemd unit is provided in `systemd/outproxy-eagle.service`; its
header comment lists the install steps (binary, config, `web/` directory).

## Web customization

The `web/` directory (configurable via `set webdir`) is served as-is:

- `web/index.html` — the page template. Placeholders like `{{TITLE}}`,
  `{{DAILY_UP}}`, `{{TOP_TOTAL}}`, `{{INFORMATION}}` are substituted on render.
- `web/style.css` — theme; edit or replace freely.
- `web/information.html` — optional; its full HTML is injected as an
  Information box. Delete it to hide the box.
- Any other file in `web/` is downloadable directly.

If `web/index.html` is missing, a built-in template is used.

> Serve the web UI behind nginx with TLS for public exposure; bind it to
> `127.0.0.1` (the default).

## License

GPLv3 (c) 2022-2026, acetone.
