#include "SmokeAnimation.h"

#include "runtime/World.h"
#include "scene/Animation.h"
#include "scene/SceneData.h"
#include "scene/SceneJson.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace {

using namespace ts::tombstone;
namespace fs = std::filesystem;

int fail(const std::string& what) {
  std::cerr << "[smoke] animation FAILED: " << what << '\n';
  return 1;
}

AnimClip grid_clip(const char* name, int start, int count, float fps,
                   AnimMode mode) {
  AnimClip c;
  c.name = name;
  c.start = start;
  c.count = count;
  c.fps = fps;
  c.mode = mode;
  return c;
}

// Frame of `clip` after each of `ticks` fixed 60 Hz ticks (index 0 = before
// the first tick), with the time summed tick by tick like the runtime does.
std::vector<int> frames_at_60hz(const AnimClip& clip, int ticks,
                                std::vector<bool>* finished = nullptr) {
  std::vector<int> out;
  double t = 0.0;
  for (int k = 0; k <= ticks; ++k) {
    const AnimSample s = sample_clip(clip, t);
    out.push_back(s.frame);
    if (finished) finished->push_back(s.finished);
    t += 1.0 / 60.0;
  }
  return out;
}

// Loop / once / ping-pong and per-frame durations, at the 60 Hz tick.
int check_timing() {
  // 10 fps = 6 ticks a frame.
  const AnimClip loop = grid_clip("walk", 0, 4, 10.0f, AnimMode::Loop);
  const std::vector<int> lf = frames_at_60hz(loop, 60);
  for (int k = 0; k <= 60; ++k) {
    if (lf[static_cast<std::size_t>(k)] != (k / 6) % 4) {
      return fail("loop frame at tick " + std::to_string(k) + " is " +
                  std::to_string(lf[static_cast<std::size_t>(k)]));
    }
  }
  const AnimClip once = grid_clip("draw", 0, 4, 10.0f, AnimMode::Once);
  std::vector<bool> done;
  const std::vector<int> of = frames_at_60hz(once, 60, &done);
  for (int k = 0; k <= 60; ++k) {
    const int want = std::min(3, k / 6);
    const bool want_done = k >= 24;
    if (of[static_cast<std::size_t>(k)] != want ||
        done[static_cast<std::size_t>(k)] != want_done) {
      return fail("once frame at tick " + std::to_string(k));
    }
  }
  // Ping-pong over 4 frames: 0 1 2 3 2 1 | 0 ...
  const AnimClip pp = grid_clip("spin", 0, 4, 10.0f, AnimMode::PingPong);
  const int seq[6] = {0, 1, 2, 3, 2, 1};
  const std::vector<int> pf = frames_at_60hz(pp, 72);
  for (int k = 0; k <= 72; ++k) {
    if (pf[static_cast<std::size_t>(k)] != seq[(k / 6) % 6]) {
      return fail("ping-pong frame at tick " + std::to_string(k));
    }
  }
  if (std::fabs(pp.cycle_seconds() - 0.6) > 1.0e-9 ||
      std::fabs(loop.cycle_seconds() - 0.4) > 1.0e-9) {
    return fail("cycle length");
  }
  // Per-frame ms: 50 / 100 / 50 ms = 3, 6, 3 ticks.
  AnimClip held = grid_clip("tip", 0, 3, 8.0f, AnimMode::Loop);
  held.ms = {50, 100, 50};
  const std::vector<int> hf = frames_at_60hz(held, 12);
  const int want_h[13] = {0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 0};
  for (int k = 0; k <= 12; ++k) {
    if (hf[static_cast<std::size_t>(k)] != want_h[k]) {
      return fail("per-frame ms at tick " + std::to_string(k));
    }
  }
  // A single-frame once clip finishes after its one frame.
  const AnimClip one = grid_clip("pose", 5, 1, 4.0f, AnimMode::Once);
  if (sample_clip(one, 0.1).finished || !sample_clip(one, 0.25).finished) {
    return fail("single-frame once clip");
  }
  return 0;
}

AnimSet sample_set() {
  AnimSet set;
  set.image = "rider.png";
  set.grid = AnimGrid{32, 48, 6, 4};
  set.default_clip = "idle_down";
  set.clips.push_back(grid_clip("idle_down", 0, 2, 2.5f, AnimMode::Loop));
  set.clips.push_back(grid_clip("walk_down", 2, 4, 8.0f, AnimMode::Loop));
  AnimClip tip;
  tip.name = "tip_hat";
  tip.grid = false;
  tip.rects = {{0, 0, 32, 48}, {32, 48, 32, 48}};
  tip.ms = {120, 240};
  tip.fps = 6.0f;
  tip.mode = AnimMode::PingPong;
  set.clips.push_back(tip);
  set.normalize();
  return set;
}

// .anim.json: write / parse roundtrip, paths, slicing, rejects, library.
int check_anim_json() {
  const AnimSet set = sample_set();
  const std::string text = anim_json::write(set);
  AnimSet back;
  std::string err;
  if (!anim_json::parse(text, &back, &err) || !(back == set) ||
      anim_json::write(back) != text) {
    return fail(".anim.json roundtrip: " + err + "\n" + text);
  }
  if (text.find("\"grid\": {\"frame_w\": 32, \"frame_h\": 48, \"cols\": 6, "
                "\"rows\": 4}") == std::string::npos ||
      text.find("{\"name\": \"walk_down\", \"fps\": 8, \"mode\": \"loop\", "
                "\"start\": 2, \"count\": 4}") == std::string::npos ||
      text.find("\"mode\": \"ping_pong\", \"frames\": [{\"x\": 0, \"y\": 0, "
                "\"w\": 32, \"h\": 48}, {\"x\": 32, \"y\": 48, \"w\": 32, "
                "\"h\": 48}], \"ms\": [120, 240]}") == std::string::npos) {
    return fail(".anim.json layout:\n" + text);
  }
  // Grid cells are row-major; explicit rects come back as written.
  const AnimClip* walk = set.find("walk_down");
  if (!walk || !(set.frame_rect(*walk, 3) == AnimRect{160, 0, 32, 48}) ||
      !(set.cell_rect(7) == AnimRect{32, 48, 32, 48}) ||
      !(set.frame_rect(*set.find("tip_hat"), 1) == AnimRect{32, 48, 32, 48}) ||
      set.start_clip() != "idle_down") {
    return fail("frame rects");
  }
  if (anim_json::set_path_for_image("assets/rider.png") !=
          "assets/rider.anim.json" ||
      anim_json::image_path("assets/rider.anim.json", set) !=
          "assets/rider.png" ||
      anim_json::image_path("assets/sheets/a.anim.json", set) !=
          "assets/sheets/rider.png") {
    return fail("anim paths");
  }
  // Re-slicing fits the grid to the image and clamps grid clips into it.
  AnimSet cut = set;
  cut.slice(64, 48, 128, 96);
  if (!(cut.grid == AnimGrid{64, 48, 2, 2}) || cut.find("walk_down")->start != 2 ||
      cut.find("walk_down")->count != 2) {
    return fail("slice");
  }
  // Junk is refused with a reason.
  AnimSet junk;
  if (anim_json::parse("{\"clips\": [{\"name\": \"a\", \"mode\": \"sideways\"}]}",
                       &junk, &err) ||
      anim_json::parse("{\"clips\": [{\"name\": \"a\"}, {\"name\": \"a\"}]}",
                       &junk, &err) ||
      err.find("duplicate clip 'a'") == std::string::npos ||
      anim_json::parse("{\"version\": 9, \"clips\": []}", &junk, &err) ||
      anim_json::parse("[]", &junk, &err)) {
    return fail("bad .anim.json should not load");
  }
  // Library: loads from the project folder, re-reads a changed file, says
  // why a missing one fails, and keeps pinned sets off the disk.
  const fs::path dir = fs::path("TombStoneProjects") / "_anim_lib_smoke";
  std::error_code ec;
  fs::remove_all(dir, ec);
  const std::string rel = "assets/rider.anim.json";
  if (!anim_json::save_file((dir / rel).string(), set, &err)) {
    return fail("save .anim.json: " + err);
  }
  AnimLibrary lib;
  const AnimLibrary::Entry* e = lib.get(dir.string(), rel);
  if (!e || !e->ok || e->image != "assets/rider.png" || !(e->set == set)) {
    return fail("library load");
  }
  AnimSet faster = set;
  faster.find("walk_down")->fps = 12.0f;
  anim_json::save_file((dir / rel).string(), faster, &err);
  fs::last_write_time(dir / rel,
                      fs::last_write_time(dir / rel, ec) + std::chrono::seconds(2),
                      ec);
  e = lib.get(dir.string(), rel);
  if (!e || !e->ok || e->set.find("walk_down")->fps != 12.0f) {
    return fail("library should re-read a changed set");
  }
  const AnimLibrary::Entry* gone = lib.get(dir.string(), "assets/nope.anim.json");
  if (!gone || gone->ok || gone->error.find("Missing .anim.json") != 0) {
    return fail("missing set should say so");
  }
  lib.put("mem/test.anim.json", sample_set());
  const AnimLibrary::Entry* pinned = lib.get(dir.string(), "mem/test.anim.json");
  if (!pinned || !pinned->ok || pinned->image != "mem/rider.png") {
    return fail("pinned set");
  }
  fs::remove_all(dir, ec);
  return 0;
}

// scene.json v5: animators round-trip; v4 files load without one.
int check_v5_format() {
  const std::string v4 =
      "{\n  \"version\": 4,\n  \"tile_solidity\": [],\n  \"entities\": [\n"
      "    {\"id\": 1, \"name\": \"Player\", \"x\": 0, \"y\": 0, \"w\": 32, "
      "\"h\": 48, \"layer\": 5, \"player\": {\"slot\": 0, \"speed\": 160}, "
      "\"collider\": {\"x\": 0, \"y\": 0, \"w\": 32, \"h\": 48, \"type\": "
      "\"solid\", \"body\": \"dynamic\"}}\n  ]\n}\n";
  scene_json::SceneDoc doc;
  std::string err;
  if (!scene_json::parse(v4, &doc, &err) || doc.version != 4 ||
      doc.entities.size() != 1 || doc.entities[0].animator ||
      !doc.entities[0].collider) {
    return fail("v4 scene should load untouched: " + err);
  }
  AnimatorData an;
  an.set = "assets/rider.anim.json";
  an.clip = "walk_right";
  an.default_clip = "idle_down";
  an.speed = 1.5f;
  an.playing = false;
  doc.entities[0].animator = an;
  const std::string v5 = scene_json::write(doc);
  scene_json::SceneDoc back;
  if (v5.find("\"version\": 5") == std::string::npos ||
      v5.find("\"animator\": {\"set\": \"assets/rider.anim.json\", \"clip\": "
              "\"walk_right\", \"default_clip\": \"idle_down\", \"speed\": 1.5, "
              "\"playing\": false}") == std::string::npos ||
      !scene_json::parse(v5, &back, &err) || back.version != 5 ||
      back.entities != doc.entities || scene_json::write(back) != v5) {
    return fail("v5 animator roundtrip: " + err + "\n" + v5);
  }
  // Speeds are clamped; a broken animator object is refused.
  if (!scene_json::parse(
          "{\"entities\": [{\"id\": 1, \"name\": \"A\", \"animator\": "
          "{\"set\": \"x.anim.json\", \"speed\": 99}}]}",
          &back, &err) ||
      back.entities[0].animator->speed != AnimatorData::kMaxSpeed ||
      scene_json::parse("{\"entities\": [{\"id\": 1, \"name\": \"A\", "
                        "\"animator\": {\"set\": }}]}",
                        &back, &err)) {
    return fail("animator clamp / reject");
  }
  return 0;
}

// A rider sheet with the given directions: idle_<d> (2 frames, 4 fps) and
// walk_<d> (4 frames, 10 fps = 6 ticks a frame) per row.
AnimSet rider_set(const std::vector<std::string>& dirs) {
  AnimSet set;
  set.image = "rider.png";
  set.grid = AnimGrid{32, 48, 6, 4};
  for (std::size_t r = 0; r < dirs.size(); ++r) {
    const int row = static_cast<int>(r) * 6;
    set.clips.push_back(grid_clip("", row, 2, 4.0f, AnimMode::Loop));
    set.clips.back().name = "idle_" + dirs[r];
    set.clips.push_back(grid_clip("", row + 2, 4, 10.0f, AnimMode::Loop));
    set.clips.back().name = "walk_" + dirs[r];
  }
  set.clips.push_back(grid_clip("tip_hat", 1, 1, 4.0f, AnimMode::Once));
  set.default_clip = set.clips.front().name;
  set.normalize();
  return set;
}

Entity2D animated_rider(std::uint64_t id, const std::string& set, int slot) {
  Entity2D e;
  e.id = id;
  e.name = "Rider " + std::to_string(id);
  e.x = 100.0f * static_cast<float>(id);
  e.w = 32.0f;
  e.h = 48.0f;
  e.player = PlayerControllerData{};
  e.player->slot = slot;
  e.animator = AnimatorData{};
  e.animator->set = set;
  return e;
}

runtime::InputFrame ride(float x, float y, int slot = 0) {
  runtime::InputFrame f;
  f.slot(slot).move_x = x;
  f.slot(slot).move_y = y;
  f.slot(slot).connected = true;
  return f;
}

std::string clip_of(const runtime::World& w, std::uint64_t id) {
  const runtime::Actor* a = w.find(id);
  return a ? a->anim.clip + (a->anim.flip_x ? " (flipped)" : "") : "?";
}

// Runtime: rider clip picking, facing memory, the flip fallback, timing on
// the tick, play_clip and the draw list.
int check_runtime() {
  runtime::World w;
  w.anim_library().put("mem/rider4.anim.json",
                       rider_set({"down", "up", "left", "right"}));
  w.anim_library().put("mem/rider1.anim.json", rider_set({"right"}));
  std::vector<Entity2D> es = {animated_rider(1, "mem/rider4.anim.json", 0),
                              animated_rider(2, "mem/rider1.anim.json", 1)};
  std::string err;
  if (!w.build(es, "", &err)) return fail("anim world: " + err);
  if (clip_of(w, 1) != "idle_down" || clip_of(w, 2) != "idle_right") {
    return fail("ride should start on the default clip: " + clip_of(w, 1));
  }
  auto steps = [&](const runtime::InputFrame& in, int n) {
    for (int i = 0; i < n; ++i) w.step(in);
  };
  // Four-way pick; idle keeps the last heading.
  struct Want {
    float x, y;
    const char* walk;
    const char* idle;
  };
  const Want ways[] = {{1, 0, "walk_right", "idle_right"},
                       {0, -1, "walk_up", "idle_up"},
                       {-1, 0, "walk_left", "idle_left"},
                       {0, 1, "walk_down", "idle_down"}};
  for (const Want& wa : ways) {
    steps(ride(wa.x, wa.y), 3);
    if (clip_of(w, 1) != wa.walk) {
      return fail(std::string("want ") + wa.walk + ", got " + clip_of(w, 1));
    }
    steps(runtime::InputFrame{}, 2);
    if (clip_of(w, 1) != wa.idle) {
      return fail(std::string("facing memory: want ") + wa.idle + ", got " +
                  clip_of(w, 1));
    }
  }
  // Diagonals keep the current heading when it is pressed, else side-on.
  steps(ride(0, -1), 1);
  steps(ride(1, -1), 5);
  if (clip_of(w, 1) != "walk_up") return fail("diagonal should keep up");
  steps(ride(-1, 0), 1);
  steps(ride(1, 1), 1);
  if (clip_of(w, 1) != "walk_right") return fail("diagonal: side-on wins");
  // Stride timing at 60 Hz: a new walk starts on frame 0, 6 ticks a frame;
  // turning mid-stride keeps the step, stopping starts the idle over.
  steps(runtime::InputFrame{}, 1);
  steps(ride(1, 0), 6);
  const runtime::Actor* a1 = w.find(1);
  if (a1->anim.frame != 1 || a1->anim.time < 0.099 || a1->anim.time > 0.101) {
    return fail("walk frame after 6 ticks: " + std::to_string(a1->anim.frame));
  }
  steps(ride(0, 1), 6);
  if (clip_of(w, 1) != "walk_down" || a1->anim.frame != 2) {
    return fail("turning should keep the stride");
  }
  steps(runtime::InputFrame{}, 1);
  if (clip_of(w, 1) != "idle_down" || a1->anim.frame != 0) {
    return fail("stopping should start the idle over");
  }
  // Right-only sheet: left is right mirrored; up / down look the way the
  // rider last went, and the mirror sticks while idle.
  runtime::InputFrame left = ride(-1, 0, 1);
  steps(left, 2);
  if (clip_of(w, 2) != "walk_right (flipped)") {
    return fail("flip fallback: " + clip_of(w, 2));
  }
  steps(runtime::InputFrame{}, 1);
  if (clip_of(w, 2) != "idle_right (flipped)") {
    return fail("flip should hold while idle: " + clip_of(w, 2));
  }
  steps(ride(0, 1, 1), 1);
  if (clip_of(w, 2) != "walk_right (flipped)") {
    return fail("down on a side-only sheet: " + clip_of(w, 2));
  }
  // The draw list shows the current frame, mirrored by swapping U.
  std::vector<runtime::DrawQuad> quads;
  auto sizes = [](const std::string& rel, int* iw, int* ih) {
    if (rel != "mem/rider.png") return false;
    *iw = 192;
    *ih = 192;
    return true;
  };
  w.build_draw_list({-5000, -5000, 5000, 5000}, 1.0f, sizes, &quads);
  const runtime::DrawQuad* q2 = nullptr;
  for (const runtime::DrawQuad& q : quads) {
    if (q.entity == 2) q2 = &q;
  }
  // walk_right on the right-only sheet: cells 2..5 of row 0, frame 0.
  if (!q2 || !q2->image || *q2->image != "mem/rider.png" || q2->missing ||
      std::fabs(q2->u1 - 64.0f / 192.0f) > 1e-6f ||
      std::fabs(q2->u0 - 96.0f / 192.0f) > 1e-6f || q2->v0 != 0.0f ||
      std::fabs(q2->v1 - 48.0f / 192.0f) > 1e-6f) {
    return fail("animated quad uv");
  }
  // play_clip by name: held clips beat the auto-pick until released;
  // unknown clips and entities say no.
  if (!w.play_clip(1, "tip_hat", true, true) || w.play_clip(1, "lasso") ||
      w.play_clip(99, "idle_down")) {
    return fail("play_clip");
  }
  steps(ride(1, 0), 20);
  if (clip_of(w, 1) != "tip_hat" || !w.find(1)->anim.finished) {
    return fail("held clip should play out and rest: " + clip_of(w, 1));
  }
  w.release_clip(1);
  steps(ride(1, 0), 1);
  if (clip_of(w, 1) != "walk_right") return fail("release_clip");
  // Paused animators hold their frame; speed scales time.
  es[0].animator->playing = false;
  es[1].animator->speed = 2.0f;
  w.build(es, "", &err);
  steps(ride(1, 0, 0), 12);
  steps(ride(1, 0, 1), 6);
  if (w.find(1)->anim.frame != 0 || w.find(1)->anim.time != 0.0 ||
      w.find(2)->anim.frame != 2) {
    return fail("playing / speed");
  }
  // A missing set falls back to the sprite, or a missing stand-in.
  Entity2D lost = animated_rider(3, "assets/nope.anim.json", 2);
  Entity2D lost_sprite = lost;
  lost_sprite.id = 4;
  lost_sprite.sprite = SpriteData{};
  lost_sprite.sprite->path = "mem/rider.png";
  w.build({lost, lost_sprite}, "", &err);
  w.step(runtime::InputFrame{});
  w.build_draw_list({-5000, -5000, 5000, 5000}, 1.0f, sizes, &quads);
  int missing = 0;
  int sprite = 0;
  for (const runtime::DrawQuad& q : quads) {
    if (q.entity == 3 && q.missing) ++missing;
    if (q.entity == 4 && !q.missing && q.image && q.u1 == 1.0f) ++sprite;
  }
  if (missing != 1 || sprite != 1 ||
      w.anim_set(*w.find(3))->error.find("Missing .anim.json") != 0) {
    return fail("missing set fallback");
  }
  return 0;
}

}  // namespace

int run_animation_smoke() {
  if (check_timing() != 0 || check_anim_json() != 0 || check_v5_format() != 0) {
    return 1;
  }
  std::cout << "[smoke] animation data OK (60 Hz timing: loop, once, "
               "ping-pong, per-frame ms; .anim.json roundtrip + rejects, "
               "grid slice, library reload, v4 -> v5 animator roundtrip)\n";
  if (check_runtime() != 0) {
    return 1;
  }
  std::cout << "[smoke] animation runtime OK (4-way idle/walk pick, facing "
               "held while idle, diagonals, stride kept on turns, right "
               "mirrored for left, frame UVs + flip in the draw list, "
               "play_clip hold/release, pause + speed, missing set "
               "fallback)\n";
  return run_animation_editor_smoke();
}
