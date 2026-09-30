#include "editor/screens/LoginScreen.h"

#include <iostream>

namespace ts {
namespace tombstone {
namespace editor {

LoginScreen::LoginScreen(std::string last_username, bool auto_login_dev)
    : auto_login_dev_(auto_login_dev),
      last_username_(std::move(last_username)) {}

void LoginScreen::on_enter() {
  authenticated_ = false;
  settings_requested_ = false;
  auto_login_attempted_ = false;
  username_ = last_username_;
  std::cout << "[Login] enter credentials (non-empty user/pass), "
               "DEV_LOGIN bypass, or open Settings\n";
  if (!last_username_.empty()) {
    std::cout << "[Login] last username: " << last_username_ << '\n';
  }
  if (auto_login_dev_) {
    std::cout << "[Login] auto_login_dev enabled -- will DEV_LOGIN on tick\n";
  }
}

void LoginScreen::on_exit() {
  if (authenticated_) {
    std::cout << "[Login] authenticated as \"" << username_ << "\"\n";
  }
}

AppState LoginScreen::on_update(float /*delta_seconds*/) {
  if (auto_login_dev_ && !auto_login_attempted_ && !authenticated_ &&
      !settings_requested_) {
    auto_login_attempted_ = true;
    submit_dev_login();
  }
  if (authenticated_) {
    return AppState::ProjectManager;
  }
  if (settings_requested_) {
    return AppState::Settings;
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
  settings_requested_ = false;
  std::cout << "[Login] success (stub credential check)\n";
  return true;
}

bool LoginScreen::submit_dev_login() {
  // DEV_LOGIN bypass for local Admin development only.
  username_ = last_username_.empty() ? "dev" : last_username_;
  authenticated_ = true;
  settings_requested_ = false;
  std::cout << "[Login] DEV_LOGIN bypass accepted as \"" << username_
            << "\"\n";
  return true;
}

void LoginScreen::request_settings() {
  settings_requested_ = true;
  std::cout << "[Login] Settings requested\n";
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
