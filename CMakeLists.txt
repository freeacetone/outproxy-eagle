cmake_minimum_required(VERSION 3.16)

# Version: -DOUTPROXY_VERSION=X (CI passes it from the git tag), else derive
# from `git describe`, else a dev fallback. A leading "v" is stripped.
if(NOT OUTPROXY_VERSION)
    execute_process(
        COMMAND git describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE OUTPROXY_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()
if(NOT OUTPROXY_VERSION)
    set(OUTPROXY_VERSION "0.0.0-dev")
endif()
string(REGEX REPLACE "^v" "" OUTPROXY_VERSION "${OUTPROXY_VERSION}")

project(outproxy-eagle VERSION 0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")

message(STATUS "outproxy-eagle version: ${OUTPROXY_VERSION}")

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
target_compile_definitions(outproxy-eagle PRIVATE
    ASIO_STANDALONE ASIO_NO_DEPRECATED
    OUTPROXY_VERSION="${OUTPROXY_VERSION}")

find_package(Threads REQUIRED)
target_link_libraries(outproxy-eagle PRIVATE Threads::Threads)

# ---------------------------------------------------------------------------
# Install rules (used by the .deb package; configure with -DCMAKE_INSTALL_PREFIX=/usr)
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS outproxy-eagle RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/web/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/outproxy-eagle/web)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
              ${CMAKE_CURRENT_SOURCE_DIR}/eagle.conf.example
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/outproxy-eagle)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/packaging/eagle.conf
        DESTINATION /etc/outproxy-eagle)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/systemd/outproxy-eagle.service
        DESTINATION /lib/systemd/system)

# ---------------------------------------------------------------------------
# CPack: Debian package
# ---------------------------------------------------------------------------
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "outproxy-eagle")
set(CPACK_PACKAGE_VERSION "${OUTPROXY_VERSION}")
set(CPACK_PACKAGE_CONTACT "acetone <63557806+freeacetone@users.noreply.github.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Lightweight native HTTP/SOCKS5 outproxy with a JS-free web UI")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/freeacetone/outproxy-eagle")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)              # auto Depends: libc6, libstdc++6
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/conffiles;${CMAKE_CURRENT_SOURCE_DIR}/packaging/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/prerm;${CMAKE_CURRENT_SOURCE_DIR}/packaging/postrm")
include(CPack)
