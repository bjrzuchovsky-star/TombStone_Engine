#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

// Simple in-memory 2D entity for the Editor2D workspace (not the runtime scene).
struct Entity2D {
  std::uint64_t id = 0;
  std::string name;
  float x = 0.0f;
  float y = 0.0f;
  float w = 64.0f;
  float h = 64.0f;
  float color[4] = {0.35f, 0.65f, 0.95f, 1.0f};  // RGBA tint
  int layer = 0;                                  // z-order (higher draws later)
};

// Flat hierarchy under a conceptual root "Scene" node.
// Persisted per project as scene.json via scene_io (load/save).
class Workspace2D {
 public:
  Workspace2D();

  void reset_defaults();  // seed Camera2D / Player / TileMap placeholders

  // Replace full scene state (used by scene.json load).
  void replace_scene(std::vector<Entity2D> entities,
                     std::optional<std::uint64_t> selected_id, float pan_x,
                     float pan_y, float zoom, bool show_grid);

  const std::vector<Entity2D>& entities() const { return entities_; }
  std::vector<Entity2D>& entities() { return entities_; }

  // --- Selection -----------------------------------------------------------
  // selected_id() is the *primary* selection (Inspector target). selection()
  // holds every selected id (primary included), in pick order.
  std::optional<std::uint64_t> selected_id() const { return selected_id_; }
  const std::vector<std::uint64_t>& selection() const { return selection_; }
  std::size_t selection_count() const { return selection_.size(); }
  bool is_selected(std::uint64_t id) const;
  void select(std::optional<std::uint64_t> id);  // replace with single / none
  void add_to_selection(std::uint64_t id);       // also becomes primary
  void toggle_selection(std::uint64_t id);       // Ctrl+click
  void set_selection(const std::vector<std::uint64_t>& ids,
                     std::optional<std::uint64_t> primary = std::nullopt);
  void select_all();
  void clear_selection();
  // Select entities whose rect intersects the world-space box. additive keeps
  // the current selection. Returns number of entities hit by the box.
  std::size_t select_in_rect(float x0, float y0, float x1, float y1,
                             bool additive);
  // Topmost entity (highest layer, then latest id) under a world point.
  std::optional<std::uint64_t> pick(float wx, float wy) const;

  Entity2D* find(std::uint64_t id);
  const Entity2D* find(std::uint64_t id) const;
  Entity2D* selected();
  const Entity2D* selected() const;

  // Create under the root Scene. Returns new id, or 0 on failure.
  std::uint64_t create_entity(std::string name = "Entity");
  bool rename_entity(std::uint64_t id, std::string name);
  bool delete_entity(std::uint64_t id);

  // --- Editing tools (operate on the whole selection) ----------------------
  // Copies every selected entity (offset by one grid cell when snapping, else
  // 16 px), selects the copies and returns their ids in order.
  std::vector<std::uint64_t> duplicate_selection();
  // Removes every selected entity. Returns how many were removed.
  std::size_t delete_selection();
  // Arrow-key nudge. dir_x/dir_y in {-1,0,1}. Step = grid (x4 when large) with
  // snap on, else 1 px (10 px when large). With snap on, positions land on
  // the grid. Returns true if anything moved.
  bool nudge_selection(int dir_x, int dir_y, bool large);
  // Snap the selection's positions to the grid. Returns true if any changed.
  bool snap_selection_to_grid();

  // Drag-move session: begin captures start positions, update applies a
  // world-space offset from those starts (snapping the primary to the grid
  // and moving the rest by the same delta), end reports whether anything
  // actually moved.
  void begin_move();
  void update_move(float dx, float dy);
  bool end_move();
  void cancel_move();  // restore start positions (Esc during a drag)
  bool move_active() const { return move_active_; }

  // Viewport camera (world space).
  float pan_x() const { return pan_x_; }
  float pan_y() const { return pan_y_; }
  float zoom() const { return zoom_; }
  void set_pan(float x, float y);
  void add_pan(float dx, float dy);
  void set_zoom(float z);
  void adjust_zoom(float factor, float anchor_screen_x, float anchor_screen_y,
                   float viewport_w, float viewport_h);

  bool show_grid() const { return show_grid_; }
  void set_show_grid(bool v) { show_grid_ = v; }
  float grid_size() const { return grid_size_; }
  void set_grid_size(float g);  // clamped to [kMinGrid, kMaxGrid]
  bool snap_enabled() const { return snap_enabled_; }
  void set_snap_enabled(bool v) { snap_enabled_ = v; }
  // Round to the nearest grid line (identity when snap is off).
  float snap_value(float v) const;
  // Round to the nearest grid line regardless of the snap toggle.
  float snap_always(float v) const;
  // Snap a size to whole grid cells (never below one cell) when snap is on.
  float snap_extent(float v) const;

  static constexpr float kMinGrid = 2.0f;
  static constexpr float kMaxGrid = 512.0f;

  // Sorted copy of entity indices by layer ascending (stable by id).
  std::vector<std::size_t> sorted_draw_order() const;

 private:
  std::uint64_t alloc_id();
  void sync_next_id_from_entities();
  static std::string strip_copy_suffix(const std::string& name);
  static std::string unique_name(const std::vector<Entity2D>& existing,
                                 std::string base);

  void prune_selection();
  float snap_step(float v, int dir, float step) const;

  struct MoveStart {
    std::uint64_t id;
    float x;
    float y;
  };

  std::vector<Entity2D> entities_;
  std::optional<std::uint64_t> selected_id_;
  std::vector<std::uint64_t> selection_;
  std::vector<MoveStart> move_starts_;
  bool move_active_ = false;
  bool move_changed_ = false;
  bool snap_enabled_ = false;
  std::uint64_t next_id_ = 1;
  float pan_x_ = 0.0f;
  float pan_y_ = 0.0f;
  float zoom_ = 1.0f;
  bool show_grid_ = true;
  float grid_size_ = 32.0f;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
