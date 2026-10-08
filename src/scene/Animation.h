#pragma once

// Sprite animation data shared by the editor, the runtime and ts_game.
// An animation set (.anim.json, kept next to its sprite sheet under
// <project>/assets/) names clips; a clip is a run of frames (grid cells or
// explicit source rects) with a rate and a play mode. Several entities can
// share one set. Pure data and timing: no UI, no graphics API.

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace ts {
namespace tombstone {

enum class AnimMode { Loop, Once, PingPong };
// "loop" / "once" / "ping_pong".
const char* to_string(AnimMode mode);
bool anim_mode_from_string(std::string_view text, AnimMode* out);

// Source rect in sheet pixels.
struct AnimRect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  bool operator==(const AnimRect& o) const = default;
};

struct AnimClip {
  static constexpr float kMinFps = 0.1f;
  static constexpr float kMaxFps = 120.0f;
  static constexpr int kMaxFrames = 1024;
  static constexpr int kMaxFrameMs = 60000;

  std::string name;
  // Grid slice: `count` cells of the set's grid starting at cell `start`
  // (row-major). Otherwise explicit source rects.
  bool grid = true;
  int start = 0;
  int count = 1;
  std::vector<AnimRect> rects;  // grid == false
  // Optional per-frame durations in ms (0 or missing = 1 / fps).
  std::vector<int> ms;
  float fps = 8.0f;
  AnimMode mode = AnimMode::Loop;

  int frame_count() const {
    return grid ? count : static_cast<int>(rects.size());
  }
  double frame_seconds(int frame) const;
  // Seconds before the clip repeats (loop / ping-pong) or ends (once).
  double cycle_seconds() const;

  bool operator==(const AnimClip& o) const = default;
};

// How the sheet is cut: frame_w x frame_h cells, cols x rows of them,
// starting at the top-left corner.
struct AnimGrid {
  static constexpr int kMaxFrameSize = 4096;
  static constexpr int kMaxCells = 256;  // per axis

  int frame_w = 32;
  int frame_h = 32;
  int cols = 1;
  int rows = 1;

  bool operator==(const AnimGrid& o) const = default;
};

struct AnimSet {
  static constexpr int kVersion = 1;

  std::string image;  // sheet, relative to the .anim.json folder
  AnimGrid grid;
  std::string default_clip;
  std::vector<AnimClip> clips;

  const AnimClip* find(std::string_view name) const;
  AnimClip* find(std::string_view name);
  int clip_index(std::string_view name) const;  // -1 when absent
  int cell_count() const { return grid.cols * grid.rows; }
  AnimRect cell_rect(int cell) const;
  // Source rect of frame `frame` (wraps into range) of `clip`.
  AnimRect frame_rect(const AnimClip& clip, int frame) const;
  // default_clip when it exists, else the first clip, else "".
  std::string start_clip() const;
  // Cut the sheet into frame_w x frame_h cells (cols / rows fitted to the
  // image; at least 1 x 1). Grid clips are clamped into the new grid.
  void slice(int frame_w, int frame_h, int image_w, int image_h);
  // Clamp sizes, rates and ranges; drop out-of-range ms entries.
  void normalize();

  bool operator==(const AnimSet& o) const = default;
};

// Where a clip is `seconds` after it started (seconds already scaled by
// playback speed). Loop wraps, once rests on its last frame, ping-pong runs
// 0..n-1 then back down to 1 and repeats.
struct AnimSample {
  int frame = 0;          // index into the clip's frames
  bool finished = false;  // a once clip resting on its last frame
};
AnimSample sample_clip(const AnimClip& clip, double seconds);

namespace anim_json {

// "assets/rider.png" -> "assets/rider.anim.json".
std::string set_path_for_image(const std::string& image_rel);
// The sheet a set at `set_rel` points at, project-relative
// ("assets/rider.anim.json" + "rider.png" -> "assets/rider.png").
std::string image_path(const std::string& set_rel, const AnimSet& set);

bool parse(const std::string& text, AnimSet* out, std::string* error_out,
           const std::string& source = ".anim.json");
// Deterministic: same set, same bytes. One clip per line.
std::string write(const AnimSet& set);
bool load_file(const std::string& path, AnimSet* out,
               std::string* error_out = nullptr);
bool save_file(const std::string& path, const AnimSet& set,
               std::string* error_out = nullptr);

}  // namespace anim_json

// Animation sets by project-relative path, read on demand from a project
// folder and re-read when the file changes on disk. Sets handed in with
// put() are pinned (tests, generated content) and never touch the disk.
// Entry addresses stay valid until clear().
class AnimLibrary {
 public:
  struct Entry {
    AnimSet set;
    std::string image;  // project-relative sheet path
    bool ok = false;
    std::string error;
  };

  // Null for an empty path. A missing or broken file gives an entry with
  // ok == false and the reason.
  const Entry* get(const std::string& project_dir, const std::string& rel);
  void put(const std::string& rel, AnimSet set);
  void forget(const std::string& rel);
  void clear() { slots_.clear(); }

 private:
  struct Slot {
    Entry entry;
    bool pinned = false;
    bool loaded = false;
    bool had_file = false;
    std::filesystem::file_time_type mtime{};
  };
  std::map<std::string, Slot> slots_;
};

}  // namespace tombstone
}  // namespace ts
