#pragma once

#include "editor/ProjectInfo.h"
#include "editor/screens/IScreen.h"
#include "editor/workspace/Workspace2D.h"

#include <cstdint>
#include <string>

namespace ts {
namespace tombstone {
namespace editor {

// 2D editor workspace: Hierarchy list, Viewport2D canvas, Inspector properties.
// Workspace is loaded from / saved to <project>/scene.json.
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
  Workspace2D& workspace() { return workspace_; }
  const Workspace2D& workspace() const { return workspace_; }

  // Persist current workspace to scene.json. Clears dirty on success.
  bool save_scene(std::string* error_out = nullptr);
  bool is_dirty() const { return dirty_; }
  const std::string& scene_path() const { return scene_path_; }

 private:
  void draw_ui();
  void draw_menu_bar();
  void draw_toolbar();
  void draw_status_bar();
  void draw_hierarchy();
  void draw_viewport();
  void draw_inspector();
  void setup_default_dock_layout(unsigned int dockspace_id);

  void begin_rename(std::uint64_t id);
  void commit_rename();
  void cancel_rename();

  void mark_dirty();
  void mark_dirty_and_autosave();

  ProjectInfo project_;
  Workspace2D workspace_;
  std::string scene_path_;
  bool dirty_ = false;
  bool quit_requested_ = false;
  bool back_requested_ = false;

  // Panel visibility (View menu).
  bool show_hierarchy_ = true;
  bool show_viewport_ = true;
  bool show_inspector_ = true;
  bool show_status_bar_ = true;
  bool show_toolbar_ = true;
  bool snap_enabled_ = false;  // placeholder UX only
  bool dock_layout_initialized_ = false;

  // Hierarchy rename state.
  bool renaming_ = false;
  std::uint64_t rename_id_ = 0;
  char rename_buf_[128]{};

  // Viewport interaction.
  bool panning_ = false;
  bool pending_click_select_ = false;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
