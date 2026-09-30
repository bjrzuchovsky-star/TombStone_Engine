#include "Physics2D.h"

namespace ts {
namespace tombstone {

bool Physics2D::init() {
  // Stub: 2D physics world not implemented yet.
  ready_ = true;
  return true;
}

void Physics2D::shutdown() {
  ready_ = false;
}

void Physics2D::step(float /*delta_seconds*/) {
  // Stub: integrate bodies / resolve contacts later.
}

}  // namespace tombstone
}  // namespace ts
