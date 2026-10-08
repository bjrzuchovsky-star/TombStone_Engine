# ENet 1.3.18 for multiplayer (ts_net -> ts_server, ts_client, ts_admin's
# local posse). The release tarball is downloaded into the build tree and
# checked by hash; nothing from it is committed here.
#
# ENet is a small C library: UDP with optional reliability, ordered
# channels, fragmentation, connection handshakes, keepalive pings and
# timeouts. It builds on Windows (Winsock) and POSIX alike. We declare our
# own target instead of using its CMakeLists so the configure step needs no
# feature probes and works on every CMake we support.

include(FetchContent)
enable_language(C)

FetchContent_Declare(
  ts_enet
  URL      https://github.com/lsalzman/enet/archive/refs/tags/v1.3.18.tar.gz
  URL_HASH SHA384=01ba23a26b79ff0f3e62d67d02b4c751575a241b9c3a90f3b5f5c3052f0d6bc5d8fbb34d42536b365081bb21e672f7c4
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  # Point at a folder with no CMakeLists.txt: populate only, never
  # add_subdirectory ENet's own build.
  SOURCE_SUBDIR _ts_populate_only
)
FetchContent_MakeAvailable(ts_enet)

set(TS_ENET_SOURCES
  callbacks.c compress.c host.c list.c packet.c peer.c protocol.c
  unix.c win32.c
)
list(TRANSFORM TS_ENET_SOURCES PREPEND ${ts_enet_SOURCE_DIR}/)

add_library(ts_enet STATIC ${TS_ENET_SOURCES})
target_include_directories(ts_enet PUBLIC ${ts_enet_SOURCE_DIR}/include)
if(WIN32)
  # win32.c is the Winsock backend; unix.c compiles to nothing here.
  target_link_libraries(ts_enet PUBLIC ws2_32 winmm)
  target_compile_definitions(ts_enet PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS
                                             _CRT_SECURE_NO_WARNINGS)
else()
  # What ENet's own configure step would find on any POSIX system we target.
  target_compile_definitions(ts_enet PRIVATE
    HAS_FCNTL=1 HAS_POLL=1 HAS_GETADDRINFO=1 HAS_GETNAMEINFO=1
    HAS_INET_PTON=1 HAS_INET_NTOP=1 HAS_MSGHDR_FLAGS=1 HAS_SOCKLEN_T=1)
endif()
set_target_properties(ts_enet PROPERTIES POSITION_INDEPENDENT_CODE ON)
# Third-party code: keep our own build warning-clean without patching it.
if(MSVC)
  target_compile_options(ts_enet PRIVATE /w)
else()
  target_compile_options(ts_enet PRIVATE -w)
endif()
