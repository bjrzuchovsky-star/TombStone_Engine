#include "scene/Animation.h"

#include "core/JsonMini.h"
#include "scene/JsonFlat.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <system_error>

namespace ts {
namespace tombstone {

namespace fs = std::filesystem;
using json_mini::escape_string;
using json_mini::match_char;
using json_mini::parse_bool;
using json_mini::parse_string;
using json_mini::skip_ws;
using namespace json_flat;

const char* to_string(AnimMode mode) {
  switch (mode) {
    case AnimMode::Once:
      return "once";
    case AnimMode::PingPong:
      return "ping_pong";
    case AnimMode::Loop:
    default:
      return "loop";
  }
}

bool anim_mode_from_string(std::string_view text, AnimMode* out) {
  if (text == "loop") {
    *out = AnimMode::Loop;
    return true;
  }
  if (text == "once") {
    *out = AnimMode::Once;
    return true;
  }
  if (text == "ping_pong" || text == "pingpong" || text == "ping-pong") {
    *out = AnimMode::PingPong;
    return true;
  }
  return false;
}

double AnimClip::frame_seconds(int frame) const {
  const int n = frame_count();
  if (n <= 0) {
    return 0.0;
  }
  frame = ((frame % n) + n) % n;
  if (frame < static_cast<int>(ms.size()) && ms[static_cast<std::size_t>(frame)] > 0) {
    return static_cast<double>(ms[static_cast<std::size_t>(frame)]) / 1000.0;
  }
  const float rate = std::clamp(fps, kMinFps, kMaxFps);
  return 1.0 / static_cast<double>(rate);
}

double AnimClip::cycle_seconds() const {
  const int n = frame_count();
  if (n <= 0) {
    return 0.0;
  }
  double total = 0.0;
  for (int i = 0; i < n; ++i) {
    total += frame_seconds(i);
  }
  if (mode == AnimMode::PingPong && n > 1) {
    for (int i = n - 2; i >= 1; --i) {
      total += frame_seconds(i);
    }
  }
  return total;
}

const AnimClip* AnimSet::find(std::string_view name) const {
  for (const AnimClip& c : clips) {
    if (c.name == name) {
      return &c;
    }
  }
  return nullptr;
}

AnimClip* AnimSet::find(std::string_view name) {
  return const_cast<AnimClip*>(
      static_cast<const AnimSet*>(this)->find(name));
}

int AnimSet::clip_index(std::string_view name) const {
  for (std::size_t i = 0; i < clips.size(); ++i) {
    if (clips[i].name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

AnimRect AnimSet::cell_rect(int cell) const {
  AnimRect r;
  r.w = std::max(1, grid.frame_w);
  r.h = std::max(1, grid.frame_h);
  if (grid.cols <= 0 || grid.rows <= 0) {
    return r;
  }
  const int total = grid.cols * grid.rows;
  cell = ((cell % total) + total) % total;
  r.x = (cell % grid.cols) * r.w;
  r.y = (cell / grid.cols) * r.h;
  return r;
}

AnimRect AnimSet::frame_rect(const AnimClip& clip, int frame) const {
  const int n = clip.frame_count();
  if (n <= 0) {
    return AnimRect{};
  }
  frame = ((frame % n) + n) % n;
  if (clip.grid) {
    return cell_rect(clip.start + frame);
  }
  return clip.rects[static_cast<std::size_t>(frame)];
}

std::string AnimSet::start_clip() const {
  if (!default_clip.empty() && find(default_clip)) {
    return default_clip;
  }
  return clips.empty() ? std::string() : clips.front().name;
}

void AnimSet::slice(int frame_w, int frame_h, int image_w, int image_h) {
  frame_w = std::clamp(frame_w, 1, AnimGrid::kMaxFrameSize);
  frame_h = std::clamp(frame_h, 1, AnimGrid::kMaxFrameSize);
  grid.frame_w = frame_w;
  grid.frame_h = frame_h;
  grid.cols = std::max(1, std::min(AnimGrid::kMaxCells,
                                   image_w > 0 ? image_w / frame_w : 1));
  grid.rows = std::max(1, std::min(AnimGrid::kMaxCells,
                                   image_h > 0 ? image_h / frame_h : 1));
  normalize();
}

void AnimSet::normalize() {
  grid.frame_w = std::clamp(grid.frame_w, 1, AnimGrid::kMaxFrameSize);
  grid.frame_h = std::clamp(grid.frame_h, 1, AnimGrid::kMaxFrameSize);
  grid.cols = std::clamp(grid.cols, 1, AnimGrid::kMaxCells);
  grid.rows = std::clamp(grid.rows, 1, AnimGrid::kMaxCells);
  const int cells = cell_count();
  for (AnimClip& c : clips) {
    c.fps = std::clamp(c.fps, AnimClip::kMinFps, AnimClip::kMaxFps);
    if (c.grid) {
      c.start = std::clamp(c.start, 0, std::max(0, cells - 1));
      c.count = std::clamp(c.count, 1, AnimClip::kMaxFrames);
      if (c.start + c.count > cells) {
        c.count = std::max(1, cells - c.start);
      }
      c.rects.clear();
    } else {
      if (c.rects.size() > static_cast<std::size_t>(AnimClip::kMaxFrames)) {
        c.rects.resize(static_cast<std::size_t>(AnimClip::kMaxFrames));
      }
      for (AnimRect& r : c.rects) {
        r.w = std::max(1, r.w);
        r.h = std::max(1, r.h);
      }
      c.start = 0;
      c.count = static_cast<int>(c.rects.size());
    }
    if (c.ms.size() > static_cast<std::size_t>(c.frame_count())) {
      c.ms.resize(static_cast<std::size_t>(c.frame_count()));
    }
    for (int& m : c.ms) {
      m = std::clamp(m, 0, AnimClip::kMaxFrameMs);
    }
  }
  if (!default_clip.empty() && !find(default_clip)) {
    default_clip.clear();
  }
}

AnimSample sample_clip(const AnimClip& clip, double seconds) {
  AnimSample s;
  const int n = clip.frame_count();
  if (n <= 0) {
    s.finished = true;
    return s;
  }
  if (!(seconds > 0.0)) {
    return s;
  }
  // Fixed ticks add up 1/60 s at a time; a hair of slack keeps a frame
  // boundary that lands exactly on a tick from rounding a tick late.
  seconds += 1.0e-9;
  if (n == 1) {
    s.finished = clip.mode == AnimMode::Once && seconds >= clip.frame_seconds(0);
    return s;
  }
  const double cycle = clip.cycle_seconds();
  if (cycle <= 0.0) {
    return s;
  }
  if (clip.mode == AnimMode::Once) {
    if (seconds >= cycle) {
      s.frame = n - 1;
      s.finished = true;
      return s;
    }
  } else {
    seconds = std::fmod(seconds, cycle);
    if (seconds < 0.0) {
      seconds += cycle;
    }
  }
  // Walk the frames (and the reverse half of a ping-pong) until `seconds`
  // lands inside one.
  auto walk = [&](int from, int to, int step) -> bool {
    for (int i = from; step > 0 ? i <= to : i >= to; i += step) {
      const double d = clip.frame_seconds(i);
      if (seconds < d) {
        s.frame = i;
        return true;
      }
      seconds -= d;
    }
    return false;
  };
  if (walk(0, n - 1, 1)) {
    return s;
  }
  if (clip.mode == AnimMode::PingPong && n > 1) {
    if (walk(n - 2, 1, -1)) {
      return s;
    }
  }
  s.frame = n - 1;
  s.finished = clip.mode == AnimMode::Once;
  return s;
}

namespace anim_json {

std::string set_path_for_image(const std::string& image_rel) {
  if (image_rel.empty()) {
    return {};
  }
  const fs::path p(image_rel);
  const fs::path stem = p.parent_path() / p.stem();
  return stem.generic_string() + ".anim.json";
}

std::string image_path(const std::string& set_rel, const AnimSet& set) {
  if (set.image.empty()) {
    return {};
  }
  const fs::path img(set.image);
  if (img.is_absolute() || set_rel.empty()) {
    return set.image;
  }
  return (fs::path(set_rel).parent_path() / img)
      .lexically_normal()
      .generic_string();
}

AnimRect rect_from_map(const FlatMap& m) {
  AnimRect r;
  r.x = map_int(m, "x", 0);
  r.y = map_int(m, "y", 0);
  r.w = map_int(m, "w", 0);
  r.h = map_int(m, "h", 0);
  return r;
}

bool parse_clip(std::string_view text, AnimClip* out, std::string* why) {
  FlatMap nested;
  std::size_t i = 0;
  auto obj = parse_flat_object(text, i, &nested);
  if (!obj) {
    if (why) *why = "bad clip object";
    return false;
  }
  AnimClip c;
  c.name = map_string(*obj, "name", "");
  if (c.name.empty()) {
    if (why) *why = "clip missing name";
    return false;
  }
  c.fps = map_float(*obj, "fps", c.fps);
  AnimMode mode = AnimMode::Loop;
  if (!anim_mode_from_string(map_string(*obj, "mode", "loop"), &mode)) {
    if (why) *why = "unknown clip mode";
    return false;
  }
  c.mode = mode;
  if (nested.count("frames")) {
    c.grid = false;
    std::vector<std::string> raw;
    if (!split_object_array(nested["frames"], &raw)) {
      if (why) *why = "bad frames array";
      return false;
    }
    for (const std::string& r : raw) {
      std::size_t j = 0;
      auto fm = parse_flat_object(r, j);
      if (!fm) {
        if (why) *why = "bad frame rect";
        return false;
      }
      c.rects.push_back(rect_from_map(*fm));
    }
  } else {
    c.grid = true;
    c.start = map_int(*obj, "start", 0);
    c.count = map_int(*obj, "count", 1);
  }
  if (nested.count("ms")) {
    if (!parse_int_array(nested["ms"], &c.ms)) {
      if (why) *why = "bad ms array";
      return false;
    }
  }
  *out = std::move(c);
  return true;
}

bool parse(const std::string& text, AnimSet* out, std::string* error_out,
           const std::string& source) {
  auto fail = [&](const std::string& what) {
    if (error_out) {
      *error_out = "Invalid .anim.json (" + what + "): " + source;
    }
    return false;
  };
  AnimSet set;
  std::size_t i = 0;
  FlatMap nested;
  auto root = parse_flat_object(text, i, &nested);
  if (!root) {
    return fail("expected object");
  }
  const int version = map_int(*root, "version", 1);
  if (version < 1 || version > AnimSet::kVersion) {
    return fail("unsupported version " + std::to_string(version));
  }
  set.image = map_string(*root, "image", "");
  if (auto g = parse_nested(nested, "grid")) {
    set.grid.frame_w = map_int(*g, "frame_w", set.grid.frame_w);
    set.grid.frame_h = map_int(*g, "frame_h", set.grid.frame_h);
    set.grid.cols = map_int(*g, "cols", set.grid.cols);
    set.grid.rows = map_int(*g, "rows", set.grid.rows);
  } else if (nested.count("grid")) {
    return fail("bad grid");
  }
  set.default_clip = map_string(*root, "default_clip", "");
  if (nested.count("clips")) {
    std::vector<std::string> raw;
    if (!split_object_array(nested["clips"], &raw)) {
      return fail("bad clips array");
    }
    for (const std::string& c : raw) {
      AnimClip clip;
      std::string why;
      if (!parse_clip(c, &clip, &why)) {
        return fail(why);
      }
      if (set.find(clip.name)) {
        return fail("duplicate clip '" + clip.name + "'");
      }
      set.clips.push_back(std::move(clip));
    }
  }
  set.normalize();
  *out = std::move(set);
  return true;
}

std::string write(const AnimSet& set_in) {
  AnimSet set = set_in;
  set.normalize();
  std::ostringstream out;
  out << "{\n";
  out << "  \"version\": " << AnimSet::kVersion << ",\n";
  out << "  \"image\": \"" << escape_string(set.image) << "\",\n";
  out << "  \"grid\": {\"frame_w\": " << set.grid.frame_w
      << ", \"frame_h\": " << set.grid.frame_h << ", \"cols\": " << set.grid.cols
      << ", \"rows\": " << set.grid.rows << "},\n";
  out << "  \"default_clip\": \"" << escape_string(set.default_clip) << "\",\n";
  out << "  \"clips\": [\n";
  for (std::size_t i = 0; i < set.clips.size(); ++i) {
    const AnimClip& c = set.clips[i];
    out << "    {\"name\": \"" << escape_string(c.name) << "\", \"fps\": "
        << format_float(c.fps) << ", \"mode\": \"" << to_string(c.mode)
        << "\"";
    if (c.grid) {
      out << ", \"start\": " << c.start << ", \"count\": " << c.count;
    } else {
      out << ", \"frames\": [";
      for (std::size_t k = 0; k < c.rects.size(); ++k) {
        const AnimRect& r = c.rects[k];
        out << (k > 0 ? ", " : "") << "{\"x\": " << r.x << ", \"y\": " << r.y
            << ", \"w\": " << r.w << ", \"h\": " << r.h << "}";
      }
      out << "]";
    }
    if (!c.ms.empty()) {
      out << ", \"ms\": [";
      for (std::size_t k = 0; k < c.ms.size(); ++k) {
        out << (k > 0 ? ", " : "") << c.ms[k];
      }
      out << "]";
    }
    out << "}" << (i + 1 < set.clips.size() ? "," : "") << "\n";
  }
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

bool load_file(const std::string& path, AnimSet* out, std::string* error_out) {
  if (path.empty()) {
    if (error_out) *error_out = "anim path is empty";
    return false;
  }
  std::error_code ec;
  if (!fs::exists(fs::path(path), ec)) {
    if (error_out) *error_out = "Missing .anim.json: " + path;
    return false;
  }
  std::ifstream in(fs::path(path), std::ios::binary);
  if (!in) {
    if (error_out) *error_out = "Could not open " + path;
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return parse(ss.str(), out, error_out, path);
}

bool save_file(const std::string& path, const AnimSet& set,
               std::string* error_out) {
  if (path.empty()) {
    if (error_out) *error_out = "anim path is empty";
    return false;
  }
  const fs::path p = fs::path(path);
  std::error_code ec;
  if (p.has_parent_path()) {
    fs::create_directories(p.parent_path(), ec);
    if (ec) {
      if (error_out) {
        *error_out =
            "Could not create directory for " + path + ": " + ec.message();
      }
      return false;
    }
  }
  const std::string text = write(set);
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  if (!out) {
    if (error_out) *error_out = "Could not write " + path;
    return false;
  }
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (!out) {
    if (error_out) *error_out = "Failed while writing " + path;
    return false;
  }
  return true;
}

}  // namespace anim_json

const AnimLibrary::Entry* AnimLibrary::get(const std::string& project_dir,
                                           const std::string& rel) {
  if (rel.empty()) {
    return nullptr;
  }
  Slot& slot = slots_[rel];
  if (slot.pinned) {
    return &slot.entry;
  }
  const fs::path path =
      project_dir.empty() ? fs::path(rel) : fs::path(project_dir) / rel;
  std::error_code ec;
  const bool exists = fs::exists(path, ec);
  const auto mtime = exists ? fs::last_write_time(path, ec)
                            : std::filesystem::file_time_type{};
  if (slot.loaded && slot.had_file == exists &&
      (!exists || slot.mtime == mtime)) {
    return &slot.entry;
  }
  slot.entry = Entry{};
  slot.loaded = true;
  slot.had_file = exists;
  slot.mtime = mtime;
  if (!exists) {
    slot.entry.error = "Missing .anim.json: " + path.string();
    return &slot.entry;
  }
  AnimSet set;
  std::string err;
  if (!anim_json::load_file(path.string(), &set, &err)) {
    slot.entry.error = err;
    return &slot.entry;
  }
  slot.entry.set = std::move(set);
  slot.entry.image = anim_json::image_path(rel, slot.entry.set);
  slot.entry.ok = true;
  return &slot.entry;
}

void AnimLibrary::put(const std::string& rel, AnimSet set) {
  if (rel.empty()) {
    return;
  }
  set.normalize();
  Slot& slot = slots_[rel];
  slot.pinned = true;
  slot.loaded = true;
  slot.entry = Entry{};
  slot.entry.ok = true;
  slot.entry.image = anim_json::image_path(rel, set);
  slot.entry.set = std::move(set);
}

void AnimLibrary::forget(const std::string& rel) {
  slots_.erase(rel);
}

}  // namespace tombstone
}  // namespace ts
