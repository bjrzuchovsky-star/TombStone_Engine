#pragma once

#include "editor/ProjectInfo.h"
#include "editor/screens/IScreen.h"

namespace ts {
namespace tombstone {
namespace editor {

// 2D editor / workspace with ImGui stub panels (viewport, hierarchy, inspector).
class Editor2DScreen final : public IScreen {
 public:
  explicit Editor2DScreen(ProjectInfo project);

  AppState state() const override { return AppState::Editor2D; }

  void on_enter() override;
  void on_exit() override;
  AppState on_update(float delta_seconds) override;

  void request_quit();
  void request_back_to_projects();

  const ProjectInfo& project() const { return project_; }

 private:
  void draw_ui();

  ProjectInfo project_;
  bool quit_requested_ = false;
  bool back_requested_ = false;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
