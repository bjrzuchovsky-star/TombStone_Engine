#pragma once

#include "editor/AppState.h"
#include "editor/ProjectInfo.h"
#include "editor/screens/IScreen.h"

#include <memory>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

// Admin UI state machine: Loading -> Login -> ProjectManager -> Editor2D.
// Owns the active screen; admin main drives ticks and login/select actions.
// Designed so each screen can grow ImGui draw code without changing the flow.
class AppFlow {
 public:
  AppFlow();
  ~AppFlow();

  AppFlow(const AppFlow&) = delete;
  AppFlow& operator=(const AppFlow&) = delete;

  void start();  // enters Loading
  void tick(float delta_seconds);

  AppState current_state() const { return state_; }
  bool is_running() const { return state_ != AppState::Quit; }

  // Login helpers (no-ops unless current state is Login).
  bool try_login(const std::string& username, const std::string& password);
  bool submit_dev_login();  // DEV_LOGIN bypass -- see LoginScreen

  // ProjectManager helpers (no-ops unless current state is ProjectManager).
  bool select_project(std::size_t index);

  // Editor2D helper.
  void request_quit();

  const std::vector<ProjectInfo>& sample_projects() const {
    return sample_projects_;
  }

  const ProjectInfo* active_project() const { return active_project_.get(); }

 private:
  void transition_to(AppState next);
  std::unique_ptr<IScreen> make_screen(AppState state) const;

  AppState state_ = AppState::Quit;
  std::unique_ptr<IScreen> screen_;
  std::vector<ProjectInfo> sample_projects_;
  std::unique_ptr<ProjectInfo> active_project_;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
