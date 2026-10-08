#pragma once

#include "editor/ProjectInfo.h"
#include "editor/screens/IScreen.h"
#include "editor/workspace/Workspace2D.h"

#include <cstddef>
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

  // Editing tools shared by hotkeys, menus, toolbar and hierarchy buttons.
  // Each autosaves scene.json like the other workspace edits.
  std::size_t duplicate_selected();  // Ctrl+D
  std::size_t delete_selected();     // Delete
  bool nudge_selected(int dir_x, int dir_y, bool large);  // arrows (+Shift)

 private:
  void draw_ui();
  void draw_menu_bar();
  void draw_toolbar();
  void draw_status_bar();
  void draw_hierarchy();
  void draw_viewport();
  void draw_inspector();
  void handle_hotkeys();
  void draw_help_menu_contents();
  // Inspector float field that live-snaps to the grid when snap is on.
  // slot identifies the field so the raw (unsnapped) drag value survives
  // between frames. Returns true when the value changed this frame.
  bool transform_field(const char* label, float* field, int slot, float vmin,
                       float vmax, bool is_extent);
  void note(std::string message);  // transient status-bar message
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
  bool dock_layout_initialized_ = false;

  // Hierarchy rename state.
  bool renaming_ = false;
  std::uint64_t rename_id_ = 0;
  char rename_buf_[128]{};

  // Viewport interaction.
  enum class DragMode { None, Move, Marquee };
  bool panning_ = false;
  DragMode drag_mode_ = DragMode::None;
  std::uint64_t drag_hit_id_ = 0;  // entity under the cursor at press (0=none)
  bool drag_additive_ = false;     // Ctrl/Shift held at press
  bool drag_moved_ = false;        // passed the drag threshold
  bool viewport_focused_ = false;
  bool hierarchy_focused_ = false;
  bool nudge_pending_save_ = false;  // autosave once arrows are released

  // Hierarchy shift-click range anchor (index into entities()).
  std::size_t range_anchor_ = 0;
  bool has_range_anchor_ = false;

  // Inspector live-snap drag state.
  int insp_drag_slot_ = -1;
  std::uint64_t insp_drag_entity_ = 0;
  float insp_drag_raw_ = 0.0f;

  // Status-bar note (e.g. "Duplicated 3").
  std::string status_note_;
  double status_note_time_ = -1000.0;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
