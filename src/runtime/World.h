#pragma once

// Runtime world: the playable copy of a scene. Built from the same
// Entity2D list the editor saves to scene.json, advanced in fixed ticks by
// abstract input, and drawn by whoever asks for a draw list (ImGui in the
// editor, OpenGL in ts_game). No UI or graphics API in here.

#include "runtime/Input.h"
#include "scene/SceneData.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace runtime {

// Simulation rate. Rendering runs at whatever the display does and
// interpolates between the last two ticks.
inline constexpr int kTickRate = 60;
inline constexpr double kTickSeconds = 1.0 / kTickRate;

// Runtime state for one entity. `data` starts as the authored entity and
// is the live copy (x/y move); the rest is state the editor never sees.
struct Actor {
  Entity2D data;
  float prev_x = 0.0f;  // position before the latest tick (interpolation)
  float prev_y = 0.0f;
  float vx = 0.0f;      // world px / s during the latest tick
  float vy = 0.0f;
  int facing = 1;       // +1 right, -1 left (sprite flip / animation later)
};

// What the camera shows: world point at the view centre plus scale
// (screen px per world px).
struct CameraView {
  float x = 0.0f;
  float y = 0.0f;
  float zoom = 1.0f;
};

struct WorldRect {
  float x0 = 0.0f;
  float y0 = 0.0f;
  float x1 = 0.0f;
  float y1 = 0.0f;
};

// One textured or solid quad in world space. image is a project-relative
// path (null = solid colour) valid until the world changes.
struct DrawQuad {
  float x0 = 0.0f;
  float y0 = 0.0f;
  float x1 = 0.0f;
  float y1 = 0.0f;
  float rgba[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  const std::string* image = nullptr;
  float u0 = 0.0f;
  float v0 = 0.0f;
  float u1 = 1.0f;
  float v1 = 1.0f;
  bool missing = false;  // sprite image unavailable: solid stand-in
  std::uint64_t entity = 0;
};

// Image dimensions for a project-relative path; false when the file is
// missing or does not decode. Lets the draw list compute UVs without
// knowing about textures.
using ImageSizeFn =
    std::function<bool(const std::string& rel_path, int* width, int* height)>;

// Absolute (or cwd-relative) path for a project-relative asset path.
std::string resolve_asset(const std::string& project_dir,
                          const std::string& rel_path);

class World {
 public:
  // Build from authored entities (copied). Each PlayerController whose
  // slot has a SpawnPoint starts centred on it. project_dir anchors the
  // project-relative image paths. False (with a reason) for an empty scene.
  bool build(const std::vector<Entity2D>& entities,
             const std::string& project_dir = std::string(),
             std::string* error_out = nullptr);
  // Read <project_dir>/scene.json and build from it.
  bool load_project(const std::string& project_dir,
                    std::string* error_out = nullptr);
  void clear();
  bool empty() const { return actors_.empty(); }
  const std::string& project_dir() const { return project_dir_; }

  // Exactly one fixed tick (kTickSeconds) with this input.
  void step(const InputFrame& input);
  std::uint64_t tick() const { return tick_; }
  double time_seconds() const { return static_cast<double>(tick_) * kTickSeconds; }

  const std::vector<Actor>& actors() const { return actors_; }
  Actor* find(std::uint64_t id);
  const Actor* find(std::uint64_t id) const;
  // First actor driven by `slot`, or null.
  const Actor* player(int slot) const;
  int player_count() const;

  // Screen size in pixels; the camera keeps its bounds with it.
  void set_view_size(float width, float height);
  // Camera between the previous and latest tick (alpha 0..1).
  CameraView camera(float alpha = 1.0f) const;
  // Entity carrying the active Camera2D (0 = implicit follow camera).
  std::uint64_t camera_entity() const { return cam_.entity; }
  std::uint64_t camera_target() const { return cam_.target; }
  // World rect a camera shows on a width x height pixel view.
  static WorldRect view_rect(const CameraView& cam, float width, float height);

  // Quads for everything visible in `view`, back to front. Cameras and
  // spawn points are editor markers and do not draw.
  void build_draw_list(const WorldRect& view, float alpha,
                       const ImageSizeFn& image_size,
                       std::vector<DrawQuad>* out) const;

 private:
  struct CameraRig {
    std::uint64_t entity = 0;  // camera entity (0 = implicit)
    std::uint64_t target = 0;  // followed entity (0 = none)
    Camera2DData data;
    float x = 0.0f;
    float y = 0.0f;
    float prev_x = 0.0f;
    float prev_y = 0.0f;
  };

  void setup_camera();
  void step_players(const InputFrame& input);
  void step_camera();
  void clamp_camera(float* x, float* y) const;
  bool target_center(float* x, float* y) const;

  std::vector<Actor> actors_;
  std::vector<std::size_t> draw_order_;
  CameraRig cam_;
  float view_w_ = 1280.0f;
  float view_h_ = 720.0f;
  std::uint64_t tick_ = 0;
  std::string project_dir_;
};

}  // namespace runtime
}  // namespace tombstone
}  // namespace ts
