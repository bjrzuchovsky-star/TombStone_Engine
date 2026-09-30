#pragma once

#include "editor/screens/IScreen.h"

namespace ts {
namespace tombstone {
namespace editor {

// Splash / loading screen. Auto-advances to Login after a short delay.
// Renders an ImGui splash with a progress bar while waiting.
class LoadingScreen final : public IScreen {
 public:
  AppState state() const override { return AppState::Loading; }

  void on_enter() override;
  void on_exit() override;
  AppState on_update(float delta_seconds) override;

 private:
  float elapsed_seconds_ = 0.0f;
  static constexpr float kMinSplashSeconds = 1.25f;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
