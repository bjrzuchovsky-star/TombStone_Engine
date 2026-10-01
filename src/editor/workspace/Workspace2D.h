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

  std::optional<std::uint64_t> selected_id() const { return selected_id_; }
  void select(std::optional<std::uint64_t> id);
  void clear_selection() { selected_id_.reset(); }

  Entity2D* find(std::uint64_t id);
  const Entity2D* find(std::uint64_t id) const;
  Entity2D* selected();
  const Entity2D* selected() const;

  // Create under the root Scene. Returns new id, or 0 on failure.
  std::uint64_t create_entity(std::string name = "Entity");
  bool rename_entity(std::uint64_t id, std::string name);
  bool delete_entity(std::uint64_t id);

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

  // Sorted copy of entity indices by layer ascending (stable by id).
  std::vector<std::size_t> sorted_draw_order() const;

 private:
  std::uint64_t alloc_id();
  void sync_next_id_from_entities();
  static std::string unique_name(const std::vector<Entity2D>& existing,
                                 std::string base);

  std::vector<Entity2D> entities_;
  std::optional<std::uint64_t> selected_id_;
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
