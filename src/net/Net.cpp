#include "Net.h"

namespace ts {
namespace tombstone {

bool Net::init() {
  // Stub: transport/protocol for a 2D four-player MMO will land here later.
  ready_ = true;
  return true;
}

void Net::shutdown() {
  ready_ = false;
}

void Net::update() {
  // Stub: tick net I/O / replication when implemented.
}

}  // namespace tombstone
}  // namespace ts
