cmake_minimum_required(VERSION 3.16)
project(outproxy-eagle CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")

add_executable(outproxy-eagle
    src/main.cpp
    src/config.cpp
    src/router.cpp
    src/upstream.cpp
    src/relay.cpp
    src/socks5.cpp
    src/http_proxy.cpp
    src/stats.cpp
    src/web.cpp
)

# standalone asio (libasio-dev) + vendored single-header Crow, both header-only.
# Resulting binary links only libc/libstdc++/pthread -> self-contained.
target_include_directories(outproxy-eagle PRIVATE third_party src)
target_compile_definitions(outproxy-eagle PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED)

find_package(Threads REQUIRED)
target_link_libraries(outproxy-eagle PRIVATE Threads::Threads)
