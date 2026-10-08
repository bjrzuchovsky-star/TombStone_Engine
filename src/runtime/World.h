#pragma once

// Runtime world: the playable copy of a scene. Built from the same
// Entity2D list the editor saves to scene.json, advanced in fixed ticks by
// abstract input, and drawn by whoever asks for a draw list (ImGui in the
// editor, OpenGL in ts_game). No UI or graphics API in here.

#include "runtime/Collision.h"
#include "runtime/Input.h"
#include "scene/Animation.h"
#include "scene/SceneData.h"
#include "scene/TileMap.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ts {
namespace tombstone {
namespace runtime {

// Simulation rate. Rendering runs at whatever the display does and
// interpolates between the last two ticks.
inline constexpr int kTickRate = 60;
inline constexpr double kTickSeconds = 1.0 / kTickRate;

// Which way a rider looks (top-down). Picks the idle_ / walk_ clip.
enum class Facing : std::uint8_t { Down, Up, Left, Right };
// "down" / "up" / "left" / "right" (clip name suffixes).
const char* to_string(Facing facing);

// Where an animator is in its set. Rebuilt with the world.
struct AnimState {
  int set = -1;          // index into the world's sets (-1 = no animation)
  std::string clip;      // clip playing now
  double time = 0.0;     // seconds into it (speed-scaled)
  int frame = 0;         // frame index within the clip
  bool finished = false; // a once clip resting on its last frame
  bool flip_x = false;   // mirrored stand-in (left from a right clip)
  bool hold = false;     // play_clip(..., hold): auto-pick keeps out
  bool walking = false;  // players: move input this tick
};

// Runtime state for one entity. `data` starts as the authored entity and
// is the live copy (x/y move); the rest is state the editor never sees.
struct Actor {
  Entity2D data;
  float prev_x = 0.0f;  // position before the latest tick (interpolation)
  float prev_y = 0.0f;
  float vx = 0.0f;      // world px / s actually moved in the latest tick
  float vy = 0.0f;      // (after walls; 0 when stopped flush)
  int facing = 1;       // +1 right, -1 left (last horizontal heading)
  Facing dir = Facing::Down;  // four-way heading; kept while idle
  float in_x = 0.0f;    // move intent of the latest tick (-1..1)
  float in_y = 0.0f;
  AnimState anim;
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

// Something walked into or out of a trigger collider. Recorded per tick
// for the scripting step to hook; nothing reacts to it yet.
struct TriggerEvent {
  enum class Kind { Enter, Exit };
  Kind kind = Kind::Enter;
  std::uint64_t trigger = 0;  // entity carrying the trigger collider
  std::uint64_t other = 0;    // body that rode in / out
  std::uint64_t tick = 0;     // the tick it happened on (1 = first tick)

  bool operator==(const TriggerEvent& o) const = default;
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
  // Same, with the scene's tile solidity (which tile ids block riders).
  bool build(const std::vector<Entity2D>& entities,
             const TileSolidity& solidity, const std::string& project_dir,
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

  // --- Collision (World_Collision.cpp) ----------------------------------------
  // Riders move one axis at a time (X, resolve, Y, resolve) against solid
  // tiles and static solid colliders, in substeps of at most half the
  // thinnest obstacle, so they slide along walls and never tunnel. Dynamic
  // bodies that end a tick overlapping are pushed apart.
  const TileSolidity& tile_solidity() const { return solidity_; }
  // Trigger enter / exit events of the latest tick (cleared every tick).
  const std::vector<TriggerEvent>& trigger_events() const { return events_; }
  const std::optional<TriggerEvent>& last_trigger_event() const {
    return last_event_;
  }
  std::uint64_t trigger_event_total() const { return events_total_; }
  bool inside_trigger(std::uint64_t trigger, std::uint64_t other) const;
  // "Player rode into Gate" (names from the scene).
  std::string describe(const TriggerEvent& event) const;
  // Collision overlay boxes visible in `view` (interpolated like the draw
  // list); triggers with somebody inside are marked active.
  void build_overlay(const WorldRect& view, float alpha,
                     std::vector<OverlayBox>* out) const;
  // Largest single move before resolution splits it (tests / overlay).
  float substep_limit() const { return step_limit_; }

  // --- Animation (World_Anim.cpp) ---------------------------------------------
  // Animators advance on the fixed tick. Riders pick idle_<dir> / walk_<dir>
  // from their move intent (falling back to fewer directions, mirroring
  // right for left, then the default clip) and keep facing while idle.
  // Sets come from the library, which outlives build() and re-reads a
  // changed .anim.json on the next build.
  AnimLibrary& anim_library() { return anim_lib_; }
  const AnimLibrary::Entry* anim_set(const Actor& a) const;
  // Play `clip` on entity `id` (restart = from frame 0 even if already on
  // it). hold keeps a rider's auto-pick off it until release_clip(). False
  // when the entity has no animator or the set has no such clip.
  bool play_clip(std::uint64_t id, const std::string& clip,
                 bool restart = true, bool hold = false);
  void release_clip(std::uint64_t id);
  // Clip a rider auto-picks for a heading (also used by the editor preview).
  // Sets *flip_x when a right clip stands in for left (or vice versa).
  static std::string pick_clip(const AnimSet& set, Facing dir, int facing,
                               bool walking, const std::string& fallback,
                               bool* flip_x);

  // Quads for everything visible in `view`, back to front. Cameras and
  // spawn points are editor markers and do not draw. Animators draw the
  // current frame of their sheet in place of the sprite image.
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

  struct SolidGrid {
    std::size_t actor = 0;  // TileMap actor (tilemaps never move)
    SolidTable table;
  };

  void setup_camera();
  void setup_collision();
  // Move by (dx, dy) with collision when the actor has a solid collider.
  void move_body(Actor& a, float dx, float dy);
  // After moving `delta` along axis (0 = x, 1 = y), back out of anything
  // solid the move entered.
  void resolve_axis(Actor& a, int axis, float delta);
  void separate_dynamics();
  void update_triggers();
  void step_players(const InputFrame& input);
  void step_camera();
  void setup_animation();
  void step_animation();
  // Switch an actor's clip; time carries over between directions of one
  // move (walk_left -> walk_up) and resets otherwise.
  static void set_clip(AnimState& st, const std::string& clip, bool flip_x,
                       bool restart);
  void clamp_camera(float* x, float* y) const;
  bool target_center(float* x, float* y) const;

  std::vector<Actor> actors_;
  std::vector<std::size_t> draw_order_;
  CameraRig cam_;
  float view_w_ = 1280.0f;
  float view_h_ = 720.0f;
  std::uint64_t tick_ = 0;
  std::string project_dir_;

  // Collision state (rebuilt by build()).
  TileSolidity solidity_;
  std::vector<SolidGrid> grids_;
  std::vector<std::size_t> static_solids_;  // actor indices
  std::vector<std::size_t> dynamics_;       // dynamic solid actors
  std::vector<std::size_t> triggers_;       // trigger actors
  float step_limit_ = 1.0e9f;
  // (trigger id, other id) pairs overlapping after the latest tick, sorted.
  std::vector<std::pair<std::uint64_t, std::uint64_t>> inside_;
  std::vector<TriggerEvent> events_;
  std::optional<TriggerEvent> last_event_;
  std::uint64_t events_total_ = 0;

  // Animation sets used by this build (copied out of the library so the
  // world stays self-contained); actors index into it.
  std::vector<AnimLibrary::Entry> anim_sets_;
  std::vector<std::string> anim_paths_;
  AnimLibrary anim_lib_;
};

}  // namespace runtime
}  // namespace tombstone
}  // namespace ts
