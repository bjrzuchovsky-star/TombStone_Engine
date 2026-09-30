#pragma once

#include "editor/screens/IScreen.h"
#include "editor/settings/Settings.h"

#include <string>

namespace ts {
namespace tombstone {
namespace editor {

// Console stub for editing Settings. Reachable from ProjectManager or Login.
// Applying a new projects_root asks AppFlow to reload the project list.
class SettingsScreen final : public IScreen {
 public:
  explicit SettingsScreen(Settings settings, AppState return_state);

  AppState state() const override { return AppState::Settings; }

  void on_enter() override;
  void on_exit() override;
  AppState on_update(float delta_seconds) override;

  const Settings& draft() const { return draft_; }
  AppState return_state() const { return return_state_; }

  // Mutators used by AppFlow / future ImGui bindings.
  void set_projects_root(std::string path);
  void set_username(std::string username);
  void set_auto_login_dev(bool enabled);
  void set_theme(std::string theme);

  // Marks draft ready to apply and leave (AppFlow persists + reloads).
  void request_apply();
  // Leave without applying draft changes.
  void request_cancel();

  bool apply_requested() const { return apply_requested_; }
  bool cancel_requested() const { return cancel_requested_; }

 private:
  Settings draft_;
  AppState return_state_ = AppState::ProjectManager;
  bool apply_requested_ = false;
  bool cancel_requested_ = false;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
