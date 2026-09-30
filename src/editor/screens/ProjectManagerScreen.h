#pragma once

#include "editor/ProjectInfo.h"
#include "editor/screens/IScreen.h"

#include <optional>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

// On-disk project list/select shell.
// Actions: Open (select index), New 2D Project, Settings, Logout.
// Selecting a 2D project opens Editor2D; 3D reports "not implemented".
class ProjectManagerScreen final : public IScreen {
 public:
  ProjectManagerScreen(std::vector<ProjectInfo> projects,
                       std::string projects_root,
                       std::string status_message = {});

  AppState state() const override { return AppState::ProjectManager; }

  void on_enter() override;
  void on_exit() override;
  AppState on_update(float delta_seconds) override;

  const std::vector<ProjectInfo>& projects() const { return projects_; }
  void set_projects(std::vector<ProjectInfo> projects);
  void set_status_message(std::string message);

  // Index into projects(). Returns false for out-of-range or unsupported kinds.
  bool select_project(std::size_t index);

  const ProjectInfo* selected_project() const;

  void request_new_project_2d(std::string name = "New 2D Project");
  void request_settings();
  void request_logout();

  bool new_project_requested() const { return new_project_requested_; }
  const std::string& pending_new_project_name() const {
    return pending_new_project_name_;
  }
  bool settings_requested() const { return settings_requested_; }
  bool logout_requested() const { return logout_requested_; }

  // Called by AppFlow after it handles new-project creation.
  void clear_new_project_request();

 private:
  std::vector<ProjectInfo> projects_;
  std::string projects_root_;
  std::string status_message_;
  std::optional<std::size_t> selected_index_;
  bool open_editor_2d_ = false;
  bool new_project_requested_ = false;
  std::string pending_new_project_name_;
  bool settings_requested_ = false;
  bool logout_requested_ = false;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
