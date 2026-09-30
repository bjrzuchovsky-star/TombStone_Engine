#pragma once

#include "editor/screens/IScreen.h"

#include <string>

namespace ts {
namespace tombstone {
namespace editor {

// Login stub -- no OAuth/backends yet.
// Accepts any non-empty username + password, OR submit_dev_login() which
// bypasses credential checks (DEV_LOGIN convenience for local Admin work).
// Settings is reachable from Login via request_settings().
class LoginScreen final : public IScreen {
 public:
  explicit LoginScreen(std::string last_username = {},
                       bool auto_login_dev = false);

  AppState state() const override { return AppState::Login; }

  void on_enter() override;
  void on_exit() override;
  AppState on_update(float delta_seconds) override;

  // Returns true and marks success when credentials are non-empty.
  bool try_login(const std::string& username, const std::string& password);

  // DEV_LOGIN bypass: treats the session as authenticated without a password.
  bool submit_dev_login();

  void request_settings();
  bool settings_requested() const { return settings_requested_; }

  const std::string& username() const { return username_; }

 private:
  bool authenticated_ = false;
  bool settings_requested_ = false;
  bool auto_login_dev_ = false;
  bool auto_login_attempted_ = false;
  std::string username_;
  std::string last_username_;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
