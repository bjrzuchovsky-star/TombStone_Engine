#include "net/Net.h"

#include <enet/enet.h>

#include <mutex>

namespace ts {
namespace tombstone {
namespace net {

namespace {
std::mutex g_mutex;
int g_users = 0;
}  // namespace

bool net_startup(std::string* error_out) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_users == 0 && enet_initialize() != 0) {
    if (error_out) *error_out = "Could not start networking (ENet / sockets).";
    return false;
  }
  ++g_users;
  return true;
}

void net_shutdown() {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_users > 0 && --g_users == 0) {
    enet_deinitialize();
  }
}

}  // namespace net
}  // namespace tombstone
}  // namespace ts
