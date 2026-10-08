#pragma once

// Networking library lifetime. The transport backend (ENet / Winsock on
// Windows) needs one process-wide start-up; Transports call these for you
// and they nest, so most code never does.

#include <string>

namespace ts {
namespace tombstone {
namespace net {

bool net_startup(std::string* error_out = nullptr);
void net_shutdown();

// Default UDP port for ts_server / ts_client (pick another with --port).
inline constexpr unsigned short kDefaultPort = 24642;

}  // namespace net
}  // namespace tombstone
}  // namespace ts
