#pragma once

#include "editor/ProjectInfo.h"
#include "editor/screens/IScreen.h"
#include "editor/workspace/Workspace2D.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

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
  std::uint64_t create_entity(std::string name = "Entity");
  bool snap_selected_to_grid();
  void reset_scene_placeholders();

  // Undo / redo (Ctrl+Z, Ctrl+Y / Ctrl+Shift+Z, Edit menu, toolbar). Each
  // restores entities + selection, autosaves scene.json and notes the step.
  bool undo();
  bool redo();

  // Coalesced edits. These are what the viewport drag and Inspector widgets
  // call; public so --smoke can drive the same paths without a window.
  // Drag-move: one undo step per whole drag.
  void begin_drag_move();
  bool end_drag_move();     // commits "Move N" + autosave if anything moved
  void cancel_drag_move();  // Esc: restore start positions, no step
  // Inspector field: before-state captured on activation, one step on
  // deactivation-after-edit.
  void begin_inspector_edit(const char* label);
  bool end_inspector_edit();
  // Commit whatever coalesced edit is open (held-arrow nudge, drag,
  // Inspector). Returns true if an undo step was pushed.
  bool flush_pending_edit();

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
  // Inspector widget hook: call right after the widget. Opens the coalesced
  // edit on activation and commits it on deactivation.
  void track_inspector_item(const char* label);
  // Discrete edit helper: flush any coalesced edit, then snapshot.
  Workspace2D::Snapshot prepare_edit();
  double now_seconds() const;
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

  // Which coalesced edit (if any) currently owns workspace_.edit_open().
  enum class EditSource { None, Drag, Inspector, Nudge };
  EditSource edit_source_ = EditSource::None;
  double last_nudge_time_ = 0.0;
  std::vector<std::uint64_t> nudge_selection_;  // who the open nudge moves
  // Primary entity as it was at the top of this frame's Inspector, so an
  // edit applied on the activation frame (color picker click) is undoable.
  Entity2D insp_frame_entity_{};
  bool insp_frame_valid_ = false;

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
