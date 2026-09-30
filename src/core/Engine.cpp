#include "Engine.h"

namespace ts {
namespace tombstone {

Engine::Engine() = default;

Engine::~Engine() {
  if (running_) {
    shutdown();
  }
}

bool Engine::init() {
  // Stub: initialize core subsystems.
  running_ = true;
  return true;
}

void Engine::shutdown() {
  // Stub: tear down core subsystems.
  running_ = false;
}

void Engine::tick(float /*delta_seconds*/) {
  // Stub: one frame of engine update.
}

}  // namespace tombstone
}  // namespace ts
