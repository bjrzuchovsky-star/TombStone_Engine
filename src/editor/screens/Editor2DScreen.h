#pragma once

#include "editor/ProjectInfo.h"
#include "editor/assets/TextureCache.h"
#include "editor/screens/IScreen.h"
#include "editor/ui/FolderBrowser.h"
#include "editor/workspace/Workspace2D.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ImDrawList;
struct ImVec2;

namespace ts {
namespace tombstone {
namespace editor {

// Viewport tools. Select keeps the original pick / drag / marquee behaviour;
// the rest paint the selected TileMap.
enum class TileTool { Select, Paint, Erase, Fill, Rect, Eyedropper };
const char* to_string(TileTool tool);

// Draw one tile: a slice of `tileset` (when non-null and id <= count) or the
// built-in palette colour. alpha scales the result (previews).
void draw_tile_quad(ImDrawList* draw, const TextureInfo* tileset,
                    int tile_size, int palette_count, int tile_id,
                    const ImVec2& p0, const ImVec2& p1, float alpha = 1.0f);

// How a sprite entity will draw this frame.
enum class SpriteState { None, Ready, Missing };

// 2D editor workspace: Hierarchy list, Viewport2D canvas, Inspector
// properties, Tile Palette and Supply Wagon (assets) panels.
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

  // --- Tile tools (Editor2DScreen_Tiles.cpp) ---------------------------------
  // All public so --smoke drives the same paths as the viewport. Every
  // committed edit is one undo step and autosaves scene.json.
  TileTool tool() const { return tool_; }
  // Picking a paint tool with no TileMap selected selects the first one.
  void set_tool(TileTool tool);
  int brush_tile() const { return brush_tile_; }
  void set_brush_tile(int tile_id);
  int brush_size() const { return brush_size_; }
  void set_brush_size(int size);  // 1..3
  // Primary selection if it is a TileMap, else 0.
  std::uint64_t paint_target() const;
  // Paint / erase stroke: begin -> stroke_to_cell (any number) -> end is
  // ONE undo step ("Paint 12 tiles"). Cells outside the grid are clipped.
  bool begin_paint_stroke(std::uint64_t tilemap_id, bool erase);
  std::size_t stroke_to_cell(int col, int row);
  bool end_paint_stroke();
  void cancel_paint_stroke();  // Esc: restore the pre-stroke tiles
  bool stroke_active() const { return stroke_id_ != 0; }
  std::size_t fill_at(std::uint64_t tilemap_id, int col, int row);  // bucket
  std::size_t fill_rect_cells(std::uint64_t tilemap_id, int c0, int r0, int c1,
                              int r1, bool erase);
  bool eyedrop(std::uint64_t tilemap_id, int col, int row);
  bool resize_tilemap(std::uint64_t tilemap_id, int cols, int rows);
  bool set_tile_size(std::uint64_t tilemap_id, int tile_size);
  bool set_tileset(std::uint64_t tilemap_id, const std::string& rel_path);
  std::uint64_t create_tilemap();
  // Number of tiles the palette offers for a TileMap (tileset slices when
  // the image loads, else the built-in 16).
  int palette_count(const Entity2D& tilemap_entity);
  bool tileset_ready(const Entity2D& tilemap_entity);

  // --- Sprites / assets --------------------------------------------------------
  // "" removes the sprite. One undo step.
  bool assign_sprite(std::uint64_t id, const std::string& rel_path);
  // Sprite entity centred on a world point, sized to the image (64 px when
  // it cannot be read). One undo step.
  std::uint64_t create_sprite_at(const std::string& rel_path, float wx,
                                 float wy);
  // Copy an image into <project>/assets and refresh the list.
  bool import_asset(const std::string& source_path,
                    std::string* rel_out = nullptr);
  void refresh_assets();
  const std::vector<std::string>& asset_list() const { return assets_; }
  std::string asset_path(const std::string& rel) const;
  const TextureInfo& texture(const std::string& rel);
  SpriteState sprite_state(const Entity2D& e);

 private:
  void draw_ui();
  void draw_menu_bar();
  void draw_toolbar();
  void draw_status_bar();
  void draw_hierarchy();
  void draw_viewport();
  void draw_inspector();
  void draw_inspector_components(Entity2D& e);
  void draw_tile_palette();
  void draw_supply_wagon();
  // Shared asset picker combo; returns true + rel_out when a choice is made
  // ("" = none).
  bool asset_combo(const char* label, const std::string& current,
                   const char* none_label, std::string* rel_out);
  // Draws a palette tile (built-in colour or tileset slice) into a rect.
  void draw_tile_swatch(ImDrawList* draw, const Entity2D* tilemap_entity,
                        int tile_id, const ImVec2& p0, const ImVec2& p1);
  void draw_tool_buttons(float button_width = 50.0f, int per_row = 6);
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
  // Discrete edit: commit + autosave + note when something changed.
  bool commit_discrete(Workspace2D::Snapshot before, const std::string& label,
                       const std::string& note_text);

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
  bool show_tile_palette_ = true;
  bool show_supply_wagon_ = true;
  bool dock_layout_initialized_ = false;

  // Hierarchy rename state.
  bool renaming_ = false;
  std::uint64_t rename_id_ = 0;
  char rename_buf_[128]{};

  // Viewport interaction.
  enum class DragMode { None, Move, Marquee, Paint, RectFill };
  bool panning_ = false;
  DragMode drag_mode_ = DragMode::None;
  std::uint64_t drag_hit_id_ = 0;  // entity under the cursor at press (0=none)
  bool drag_additive_ = false;     // Ctrl/Shift held at press
  bool drag_moved_ = false;        // passed the drag threshold
  bool viewport_focused_ = false;
  bool hierarchy_focused_ = false;
  bool nudge_pending_save_ = false;  // autosave once arrows are released

  // Which coalesced edit (if any) currently owns workspace_.edit_open().
  enum class EditSource { None, Drag, Inspector, Nudge, Paint };
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

  // Tile tools.
  TileTool tool_ = TileTool::Select;
  int brush_tile_ = 1;
  int brush_size_ = 1;
  std::uint64_t stroke_id_ = 0;
  bool stroke_erase_ = false;
  bool stroke_has_last_ = false;
  int stroke_last_col_ = 0;
  int stroke_last_row_ = 0;
  std::size_t stroke_cells_ = 0;
  int rect_c0_ = 0;
  int rect_r0_ = 0;
  // Hover readout for the status bar.
  bool hover_cell_valid_ = false;
  int hover_col_ = 0;
  int hover_row_ = 0;
  int hover_tile_ = 0;

  // Assets (<project>/assets) + texture cache (GL handles in the admin app,
  // size-only when headless).
  std::vector<std::string> assets_;
  double assets_refresh_time_ = -1000.0;
  TextureCache textures_;
  FolderBrowser import_browser_;
  std::string wagon_selected_;  // highlighted asset in the Supply Wagon

  // Status-bar note (e.g. "Duplicated 3").
  std::string status_note_;
  double status_note_time_ = -1000.0;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
