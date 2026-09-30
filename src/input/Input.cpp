#include "Input.h"

namespace ts {
namespace tombstone {

bool Input::init() {
  // Stub: keyboard/mouse/gamepad routing later.
  ready_ = true;
  return true;
}

void Input::shutdown() {
  ready_ = false;
}

void Input::update() {
  // Stub: sample device state each frame.
}

}  // namespace tombstone
}  // namespace ts
