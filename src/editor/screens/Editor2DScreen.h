#pragma once

#include "editor/ProjectInfo.h"
#include "editor/screens/IScreen.h"

namespace ts {
namespace tombstone {
namespace editor {

// 2D editor / workspace stub. Panel names are placeholders for future ImGui
// docking (viewport, hierarchy, inspector, toolbar).
class Editor2DScreen final : public IScreen {
 public:
  explicit Editor2DScreen(ProjectInfo project);

  AppState state() const override { return AppState::Editor2D; }

  void on_enter() override;
  void on_exit() override;
  AppState on_update(float delta_seconds) override;

  void request_quit();
  void request_back_to_projects();  // return to ProjectManager

  const ProjectInfo& project() const { return project_; }

 private:
  ProjectInfo project_;
  bool quit_requested_ = false;
  bool back_requested_ = false;
  bool panels_dumped_ = false;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
