#include "editor/screens/LoginScreen.h"

#include <iostream>

namespace ts {
namespace tombstone {
namespace editor {

void LoginScreen::on_enter() {
  authenticated_ = false;
  username_.clear();
  std::cout << "[Login] enter credentials (non-empty user/pass) "
               "or use DEV_LOGIN bypass\n";
}

void LoginScreen::on_exit() {
  std::cout << "[Login] authenticated as \"" << username_ << "\"\n";
}

AppState LoginScreen::on_update(float /*delta_seconds*/) {
  if (authenticated_) {
    return AppState::ProjectManager;
  }
  return AppState::Login;
}

bool LoginScreen::try_login(const std::string& username,
                            const std::string& password) {
  // Stub auth: any non-empty username and password succeed.
  // No network, OAuth, or credential store -- hang a real backend later.
  if (username.empty() || password.empty()) {
    std::cout << "[Login] rejected: username and password must be non-empty\n";
    return false;
  }
  username_ = username;
  authenticated_ = true;
  std::cout << "[Login] success (stub credential check)\n";
  return true;
}

bool LoginScreen::submit_dev_login() {
  // DEV_LOGIN bypass for local Admin development only.
  username_ = "dev";
  authenticated_ = true;
  std::cout << "[Login] DEV_LOGIN bypass accepted\n";
  return true;
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
