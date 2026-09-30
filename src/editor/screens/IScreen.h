#pragma once

#include "editor/AppState.h"

namespace ts {
namespace tombstone {
namespace editor {

// Screen contract intended for ImGui (or similar) panels later.
// Console stubs implement the same hooks so the AppFlow state machine
// stays backend-agnostic.
class IScreen {
 public:
  virtual ~IScreen() = default;

  virtual AppState state() const = 0;

  virtual void on_enter() = 0;
  virtual void on_exit() = 0;

  // Returns the state to transition to, or the current state to stay.
  virtual AppState on_update(float delta_seconds) = 0;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
