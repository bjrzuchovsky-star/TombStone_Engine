#include "editor/screens/LoadingScreen.h"

#include <iostream>

namespace ts {
namespace tombstone {
namespace editor {

void LoadingScreen::on_enter() {
  elapsed_seconds_ = 0.0f;
  std::cout << "[Loading] splash / asset warm-up (stub)\n";
}

void LoadingScreen::on_exit() {
  std::cout << "[Loading] done\n";
}

AppState LoadingScreen::on_update(float delta_seconds) {
  elapsed_seconds_ += delta_seconds;
  if (elapsed_seconds_ >= kMinSplashSeconds) {
    return AppState::Login;
  }
  return AppState::Loading;
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
