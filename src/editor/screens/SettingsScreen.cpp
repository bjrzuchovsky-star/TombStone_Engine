#include "editor/screens/SettingsScreen.h"

#include <iostream>

namespace ts {
namespace tombstone {
namespace editor {

SettingsScreen::SettingsScreen(Settings settings, AppState return_state)
    : draft_(std::move(settings)), return_state_(return_state) {}

void SettingsScreen::on_enter() {
  apply_requested_ = false;
  cancel_requested_ = false;
  std::cout << "[Settings] edit config (cwd-relative defaults)\n";
  std::cout << "  projects_root   = " << draft_.projects_root << '\n';
  std::cout << "  username        = " << draft_.username << '\n';
  std::cout << "  auto_login_dev  = " << (draft_.auto_login_dev ? "true" : "false")
            << '\n';
  std::cout << "  theme           = " << draft_.theme << '\n';
  std::cout << "  last_project    = " << draft_.last_project_path << '\n';
  std::cout << "  config file     = " << SettingsStore::default_settings_path()
            << '\n';
}

void SettingsScreen::on_exit() {
  std::cout << "[Settings] leaving\n";
}

AppState SettingsScreen::on_update(float /*delta_seconds*/) {
  if (apply_requested_ || cancel_requested_) {
    return return_state_;
  }
  return AppState::Settings;
}

void SettingsScreen::set_projects_root(std::string path) {
  draft_.projects_root = std::move(path);
}

void SettingsScreen::set_username(std::string username) {
  draft_.username = std::move(username);
}

void SettingsScreen::set_auto_login_dev(bool enabled) {
  draft_.auto_login_dev = enabled;
}

void SettingsScreen::set_theme(std::string theme) {
  draft_.theme = std::move(theme);
}

void SettingsScreen::request_apply() {
  apply_requested_ = true;
  cancel_requested_ = false;
  std::cout << "[Settings] apply requested\n";
}

void SettingsScreen::request_cancel() {
  cancel_requested_ = true;
  apply_requested_ = false;
  std::cout << "[Settings] cancel requested\n";
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
