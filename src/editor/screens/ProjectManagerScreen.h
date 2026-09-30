#pragma once

#include "editor/ProjectInfo.h"
#include "editor/screens/IScreen.h"

#include <optional>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

// In-memory project list/select shell. Selecting a 2D project opens Editor2D;
// 3D projects report "not implemented" and stay on this screen.
class ProjectManagerScreen final : public IScreen {
 public:
  explicit ProjectManagerScreen(std::vector<ProjectInfo> projects);

  AppState state() const override { return AppState::ProjectManager; }

  void on_enter() override;
  void on_exit() override;
  AppState on_update(float delta_seconds) override;

  const std::vector<ProjectInfo>& projects() const { return projects_; }

  // Index into projects(). Returns false for out-of-range or unsupported kinds.
  bool select_project(std::size_t index);

  const ProjectInfo* selected_project() const;

 private:
  std::vector<ProjectInfo> projects_;
  std::optional<std::size_t> selected_index_;
  bool open_editor_2d_ = false;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
