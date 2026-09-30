#pragma once

#include "editor/AppState.h"
#include "editor/ProjectInfo.h"
#include "editor/projects/ProjectStore.h"
#include "editor/screens/IScreen.h"
#include "editor/settings/Settings.h"

#include <memory>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

// Admin UI state machine:
//   Loading -> Login -> ProjectManager -> Editor2D
//                    \-> Settings <-> (Login | ProjectManager)
// Owns settings + on-disk project store; admin main drives ticks/actions.
class AppFlow {
 public:
  AppFlow();
  ~AppFlow();

  AppFlow(const AppFlow&) = delete;
  AppFlow& operator=(const AppFlow&) = delete;

  void start();  // loads settings, refreshes projects, enters Loading
  void tick(float delta_seconds);

  AppState current_state() const { return state_; }
  bool is_running() const { return state_ != AppState::Quit; }

  const Settings& settings() const { return settings_; }
  const std::string& last_error() const { return last_error_; }

  // Login helpers (no-ops unless current state is Login).
  bool try_login(const std::string& username, const std::string& password);
  bool submit_dev_login();  // DEV_LOGIN bypass -- see LoginScreen
  bool open_settings_from_login();

  // ProjectManager helpers.
  bool select_project(std::size_t index);  // Open
  bool create_new_project_2d(const std::string& name = "New 2D Project");
  bool open_settings_from_projects();
  bool logout();

  // Settings helpers.
  bool settings_set_projects_root(std::string path);
  bool settings_set_username(std::string username);
  bool settings_set_auto_login_dev(bool enabled);
  bool settings_set_theme(std::string theme);
  bool apply_settings_draft();  // persist + reload projects if root changed
  bool cancel_settings();

  // Editor2D helpers.
  void request_back_to_projects();
  void request_quit();

  const std::vector<ProjectInfo>& projects() const {
    return project_store_.projects();
  }

  const ProjectInfo* active_project() const { return active_project_.get(); }

 private:
  void transition_to(AppState next);
  std::unique_ptr<IScreen> make_screen(AppState state);
  bool reload_projects();
  void persist_settings();
  void handle_pending_screen_actions();

  AppState state_ = AppState::Quit;
  AppState settings_return_state_ = AppState::ProjectManager;
  std::unique_ptr<IScreen> screen_;
  Settings settings_;
  ProjectStore project_store_;
  std::unique_ptr<ProjectInfo> active_project_;
  std::string status_message_;
  std::string last_error_;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
