#include "SmokeCollision.h"

#include "editor/ProjectInfo.h"
#include "editor/screens/Editor2DScreen.h"
#include "editor/workspace/SceneIO.h"
#include "runtime/Collision.h"
#include "runtime/PlaySession.h"
#include "runtime/World.h"
#include "scene/SceneData.h"
#include "scene/SceneJson.h"
#include "scene/TileMap.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace {

using namespace ts::tombstone;

int fail(const std::string& what) {
  std::cerr << "[smoke] collision FAILED: " << what << '\n';
  return 1;
}

bool near(float a, float b, float eps = 0.01f) {
  return std::fabs(a - b) <= eps;
}

runtime::InputFrame ride(float x, float y, int slot = 0) {
  runtime::InputFrame f;
  f.slot(slot).move_x = x;
  f.slot(slot).move_y = y;
  f.slot(slot).connected = true;
  return f;
}

Entity2D make_rider(std::uint64_t id, float x, float y, int slot = 0,
                    float speed = 160.0f) {
  Entity2D e;
  e.id = id;
  e.name = slot == 0 ? "Rider" : "Rider " + std::to_string(slot + 1);
  e.x = x;
  e.y = y;
  e.w = 32.0f;
  e.h = 32.0f;
  e.player = PlayerControllerData{slot, speed};
  e.collider = default_collider(e);
  return e;
}

Entity2D make_box(std::uint64_t id, const char* name, float x, float y,
                  float w, float h, bool trigger, bool dynamic) {
  Entity2D e;
  e.id = id;
  e.name = name;
  e.x = x;
  e.y = y;
  e.w = w;
  e.h = h;
  e.collider = default_collider(e);
  e.collider->trigger = trigger;
  e.collider->dynamic = dynamic;
  return e;
}

// 12 x 10 grid of 32 px Dirt (walkable) with Stone (solid) in `wall_col`.
Entity2D make_ground(std::uint64_t id, int wall_col) {
  Entity2D e;
  e.id = id;
  e.name = "Ground";
  e.tilemap = TileMapData(12, 10, 32);
  for (int r = 0; r < 10; ++r) {
    for (int c = 0; c < 12; ++c) {
      e.tilemap->set(c, r, c == wall_col ? 3 : 1);
    }
  }
  sync_tilemap_extent(e);
  return e;
}

float right_edge(const runtime::Actor* a) {
  return runtime::collider_box(a->data).x1;
}

// Solid tiles, wall sliding, tunnelling, static crates, overrides.
int check_walls() {
  // 1) Ride right into a Stone column at x = 192: stop flush, speed 0.
  {
    runtime::World w;
    w.build({make_ground(1, 6), make_rider(2, 64.0f, 64.0f)});
    for (int i = 0; i < 120; ++i) {
      w.step(ride(1.0f, 0.0f));
    }
    const runtime::Actor* p = w.player(0);
    if (!p || !near(right_edge(p), 192.0f, 1.0e-3f) || !near(p->vx, 0.0f) ||
        !near(p->data.y, 64.0f, 1.0e-4f)) {
      return fail("rider should stop flush on a solid tile (right edge " +
                  std::to_string(p ? right_edge(p) : 0.0f) + ")");
    }
    // 2) Push diagonally into the wall: X stays flush, Y keeps the
    // parallel part of the motion (speed / sqrt 2 for half a second).
    const float y0 = p->data.y;
    for (int i = 0; i < 30; ++i) {
      w.step(ride(1.0f, 1.0f));
    }
    const float want = 160.0f / std::sqrt(2.0f) * 0.5f;
    if (!near(right_edge(p), 192.0f, 1.0e-3f) ||
        !near(p->data.y - y0, want, 0.05f)) {
      return fail("rider should slide along the wall (dy " +
                  std::to_string(p->data.y - y0) + ", want " +
                  std::to_string(want) + ")");
    }
    // Overlay (K): one merged run per wall row + the rider's box.
    std::vector<runtime::OverlayBox> boxes;
    w.build_overlay({-1000.0f, -1000.0f, 1000.0f, 1000.0f}, 1.0f, &boxes);
    int tiles = 0;
    int dyn = 0;
    for (const runtime::OverlayBox& b : boxes) {
      if (b.kind == runtime::OverlayKind::SolidTile && b.x0 == 192.0f &&
          b.x1 == 224.0f) {
        ++tiles;
      }
      if (b.kind == runtime::OverlayKind::DynamicSolid && b.entity == 2) {
        ++dyn;
      }
    }
    if (tiles != 10 || dyn != 1 || boxes.size() != 11) {
      return fail("collision overlay boxes (" + std::to_string(boxes.size()) +
                  ")");
    }
  }
  // 3) 5000 px/s (83 px a tick, more than two tiles) never tunnels:
  // through a one-tile Stone wall, or a 4 px static fence.
  {
    runtime::World w;
    w.build({make_ground(1, 5), make_rider(2, 32.0f, 0.0f, 0, 5000.0f)});
    for (int i = 0; i < 10; ++i) {
      w.step(ride(1.0f, 0.0f));
    }
    if (!near(right_edge(w.player(0)), 160.0f, 1.0e-3f)) {
      return fail("fast rider tunnelled a tile wall (right edge " +
                  std::to_string(right_edge(w.player(0))) + ")");
    }
    runtime::World f;
    f.build({make_rider(2, 200.0f, 0.0f, 0, 5000.0f),
             make_box(3, "Fence", 300.0f, -200.0f, 4.0f, 400.0f, false,
                      false)});
    f.step(ride(1.0f, 0.0f));
    if (!near(right_edge(f.player(0)), 300.0f, 1.0e-3f) ||
        !(f.substep_limit() <= 2.0f)) {
      return fail("fast rider tunnelled a 4 px fence (right edge " +
                  std::to_string(right_edge(f.player(0))) + ")");
    }
  }
  // 4) Static solid crate blocks; a crate with no collider does not; a
  // dynamic crate gets shoved along.
  {
    runtime::World w;
    w.build({make_rider(1, 100.0f, 0.0f),
             make_box(2, "Crate", 200.0f, 0.0f, 32.0f, 32.0f, false, false)});
    for (int i = 0; i < 90; ++i) {
      w.step(ride(1.0f, 0.0f));
    }
    if (!near(right_edge(w.player(0)), 200.0f, 1.0e-3f) ||
        w.find(2)->data.x != 200.0f) {
      return fail("static crate should block and stay put");
    }
    Entity2D ghost = make_box(2, "Ghost", 200.0f, 0.0f, 32.0f, 32.0f, false,
                              false);
    ghost.collider.reset();
    runtime::World g;
    g.build({make_rider(1, 100.0f, 0.0f), ghost});
    for (int i = 0; i < 90; ++i) {
      g.step(ride(1.0f, 0.0f));
    }
    if (!near(g.player(0)->data.x, 100.0f + 160.0f * 1.5f, 0.05f)) {
      return fail("crate without a collider should not block");
    }
    runtime::World d;
    d.build({make_rider(1, 100.0f, 0.0f),
             make_box(2, "Barrel", 200.0f, 0.0f, 32.0f, 32.0f, false, true)});
    for (int i = 0; i < 90; ++i) {
      d.step(ride(1.0f, 0.0f));
    }
    const runtime::Actor* barrel = d.find(2);
    if (!(barrel->data.x > 230.0f) ||
        runtime::overlaps(runtime::collider_box(d.player(0)->data),
                          runtime::collider_box(barrel->data))) {
      return fail("dynamic crate should be shoved, not overlapped");
    }
  }
  // 5) Project override: Dirt made solid blocks; Stone made walkable
  // doesn't (scene.json tile_solidity).
  {
    TileSolidity ts;
    ts.set_solid("", 1, true);
    ts.set_solid("", 3, false);
    Entity2D ground = make_ground(1, 6);
    for (int c = 0; c < 12; ++c) {
      ground.tilemap->set(c, 0, 4);  // a Grass lane on row 0
    }
    ground.tilemap->set(9, 0, 1);  // one Dirt block in the lane
    runtime::PlaySession s;
    if (!s.start({ground, make_rider(2, 64.0f, 0.0f)}, ts, "")) {
      return fail("session with tile solidity");
    }
    s.run_ticks(180, ride(1.0f, 0.0f));
    if (!near(right_edge(s.world().player(0)), 288.0f, 1.0e-3f) ||
        !s.world().tile_solidity().solid("", 1)) {
      return fail("tile_solidity overrides (right edge " +
                  std::to_string(right_edge(s.world().player(0))) + ")");
    }
  }
  return 0;
}

// Dynamic vs dynamic: riders push apart and never end a tick overlapped.
int check_riders() {
  runtime::World w;
  w.build({make_rider(1, 100.0f, 100.0f, 0), make_rider(2, 110.0f, 100.0f, 1)});
  w.step(runtime::InputFrame{});
  const runtime::Actor* a = w.player(0);
  const runtime::Actor* b = w.player(1);
  if (!near(a->data.x, 89.0f) || !near(b->data.x, 121.0f) ||
      runtime::overlaps(runtime::collider_box(a->data),
                        runtime::collider_box(b->data))) {
    return fail("overlapping riders should separate half each (" +
                std::to_string(a->data.x) + ", " + std::to_string(b->data.x) +
                ")");
  }
  for (int i = 0; i < 60; ++i) {
    runtime::InputFrame f = ride(1.0f, 0.0f, 0);
    f.slot(1).move_x = -1.0f;
    f.slot(1).connected = true;
    w.step(f);
    if (runtime::overlaps(runtime::collider_box(a->data),
                          runtime::collider_box(b->data))) {
      return fail("riders overlapped after tick " + std::to_string(i));
    }
  }
  return 0;
}

// Trigger enter / exit fire exactly once each, on the right ticks, and the
// trigger never blocks.
int check_triggers() {
  runtime::World w;
  w.build({make_rider(1, 100.0f, 0.0f),
           make_box(2, "Gate", 200.0f, -16.0f, 64.0f, 64.0f, true, false)});
  int enters = 0;
  int exits = 0;
  std::uint64_t enter_tick = 0;
  std::uint64_t exit_tick = 0;
  bool active_seen = false;
  for (int i = 0; i < 120; ++i) {
    w.step(ride(1.0f, 0.0f));
    for (const runtime::TriggerEvent& ev : w.trigger_events()) {
      if (ev.trigger != 2 || ev.other != 1 || ev.tick != w.tick()) {
        return fail("trigger event fields");
      }
      if (ev.kind == runtime::TriggerEvent::Kind::Enter) {
        ++enters;
        enter_tick = ev.tick;
      } else {
        ++exits;
        exit_tick = ev.tick;
      }
    }
    if (w.inside_trigger(2, 1) && !active_seen) {
      std::vector<runtime::OverlayBox> boxes;
      w.build_overlay({0.0f, -100.0f, 400.0f, 100.0f}, 1.0f, &boxes);
      for (const runtime::OverlayBox& b : boxes) {
        active_seen |= b.kind == runtime::OverlayKind::Trigger && b.active;
      }
    }
  }
  // Rider right edge 132 -> crosses x = 200 on tick 26; left edge 100
  // passes x = 264 on tick 62.
  if (enters != 1 || exits != 1 || enter_tick != 26 || exit_tick != 62 ||
      !active_seen || w.trigger_event_total() != 2 ||
      !w.last_trigger_event() ||
      w.last_trigger_event()->kind != runtime::TriggerEvent::Kind::Exit ||
      w.describe(*w.last_trigger_event()) != "Rider rode out of Gate" ||
      !near(w.player(0)->data.x, 100.0f + 320.0f, 0.05f)) {
    return fail("trigger enter/exit (" + std::to_string(enters) + "@" +
                std::to_string(enter_tick) + ", " + std::to_string(exits) +
                "@" + std::to_string(exit_tick) + ")");
  }
  return 0;
}

// v4: colliders + per-tileset tile solidity, and the v3 -> v4 upgrade.
int check_v4_format() {
  std::string err;
  // A v3 file: the rider gains a default dynamic collider (its whole rect),
  // other entities stay collider-free, tile solidity starts at defaults.
  const std::string v3 =
      "{\n  \"version\": 3,\n  \"entities\": [\n"
      "    {\"id\": 1, \"name\": \"Rider\", \"x\": 10, \"y\": 20, \"w\": 30, "
      "\"h\": 40, \"layer\": 5, \"player\": {\"slot\": 1, \"speed\": 200}},\n"
      "    {\"id\": 2, \"name\": \"Crate\", \"x\": 100, \"y\": 0, \"w\": 32, "
      "\"h\": 32, \"layer\": 1}\n"
      "  ]\n}\n";
  scene_json::SceneDoc doc;
  if (!scene_json::parse(v3, &doc, &err) || doc.version != 3) {
    return fail("v3 parse: " + err);
  }
  const Entity2D& rider = doc.entities[0];
  if (!rider.collider || rider.collider->w != 30.0f ||
      rider.collider->h != 40.0f || rider.collider->offset_x != 0.0f ||
      !rider.collider->dynamic || rider.collider->trigger ||
      doc.entities[1].collider || !doc.tile_solidity.overrides.empty()) {
    return fail("v3 -> v4 upgrade should give riders a default collider");
  }
  // Built-in defaults: Stone / Water / Wood block, Dirt / Sand / Grass
  // don't; image tilesets start walkable; stale ids wrap like the palette.
  const TileSolidity defaults;
  if (!defaults.solid("", 3) || !defaults.solid("", 5) ||
      !defaults.solid("", 6) || defaults.solid("", 1) ||
      defaults.solid("", 2) || defaults.solid("", 4) ||
      defaults.solid("assets/tiles.png", 3) || !defaults.solid("", 19) ||
      defaults.solid("", 0)) {
    return fail("built-in tile solidity defaults");
  }
  // Edits: an override that returns to the defaults disappears.
  TileSolidity ts = defaults;
  if (!ts.set_solid("", 1, true) || ts.set_solid("", 1, true) ||
      !ts.solid("", 1) || ts.overrides.size() != 1 ||
      !ts.set_solid("", 1, false) || !ts.overrides.empty()) {
    return fail("tile solidity toggle");
  }
  ts.set_solid("", 4, true);    // Grass becomes a hedge
  ts.set_solid("", 5, false);   // Water becomes a ford
  ts.set_solid("assets/tiles.png", 7, true);
  doc.tile_solidity = ts;
  // Every collider field round-trips; triggers and statics stay put.
  Entity2D gate;
  gate.id = 3;
  gate.name = "Gate";
  gate.collider = ColliderData{4.5f, -2.0f, 24.0f, 8.25f, true, false};
  doc.entities.push_back(gate);
  doc.entities[1].collider = default_collider(doc.entities[1]);
  const std::string v4 = scene_json::write(doc);
  scene_json::SceneDoc back;
  if (v4.find("\"version\": 4") == std::string::npos ||
      v4.find("\"collider\": {\"x\": 0, \"y\": 0, \"w\": 30, \"h\": 40, "
              "\"type\": \"solid\", \"body\": \"dynamic\"}") ==
          std::string::npos ||
      v4.find("\"type\": \"trigger\", \"body\": \"static\"") ==
          std::string::npos ||
      v4.find("{\"tileset\": \"\", \"solid\": [3, 4, 6, 8, 11, 12, 14, 15]}") ==
          std::string::npos ||
      v4.find("{\"tileset\": \"assets/tiles.png\", \"solid\": [7]}") ==
          std::string::npos) {
    return fail("v4 write:\n" + v4);
  }
  if (!scene_json::parse(v4, &back, &err) || back.version != 4 ||
      back.entities != doc.entities || back.tile_solidity != ts ||
      scene_json::write(back) != v4) {
    return fail("v4 roundtrip: " + err);
  }
  // A v4 rider without a collider stays a ghost (no re-upgrade).
  back.entities[0].collider.reset();
  scene_json::SceneDoc again;
  if (!scene_json::parse(scene_json::write(back), &again, &err) ||
      again.entities[0].collider) {
    return fail("v4 files must not be re-upgraded");
  }
  // Junk is rejected with a reason.
  if (scene_json::parse("{\"entities\": [{\"id\": 1, \"name\": \"A\", "
                        "\"collider\": {\"type\": \"lava\"}}]}",
                        &again, &err) ||
      scene_json::parse("{\"tile_solidity\": [{\"tileset\": \"\", "
                        "\"solid\": [1, \"x\"]}], \"entities\": []}",
                        &again, &err)) {
    return fail("bad collider / tile_solidity should not load");
  }
  return 0;
}

std::string read_text(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

int count_kind(const std::vector<runtime::OverlayBox>& boxes,
               runtime::OverlayKind kind) {
  int n = 0;
  for (const runtime::OverlayBox& b : boxes) {
    n += b.kind == kind ? 1 : 0;
  }
  return n;
}

// The editor side, headless: palette solid toggle and collider edits are
// single undo steps that land in scene.json; Play rides on the edited
// solidity; the K overlay and the trigger readout see the same world.
int check_editor() {
  namespace fs = std::filesystem;
  const fs::path dir = fs::path("TombStoneProjects") / "_collision_smoke";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  editor::ProjectInfo info;
  info.id = "_collision_smoke";
  info.name = "Collision Smoke";
  info.path = dir.string();
  editor::Editor2DScreen scr(info);
  scr.on_enter();
  editor::Workspace2D& w = scr.workspace();
  const fs::path scene = editor::scene_io::scene_path_for_project(info.path);
  const Entity2D* player = find_entity_named(w.entities(), "Player");
  if (!player || !player->collider || !player->collider->dynamic ||
      read_text(scene).find("\"collider\": {\"x\": 0, \"y\": 0, \"w\": 32, "
                            "\"h\": 48, \"type\": \"solid\", \"body\": "
                            "\"dynamic\"}") == std::string::npos ||
      read_text(scene).find("\"tile_solidity\": []") == std::string::npos) {
    return fail("seed Player should ride with a default dynamic collider");
  }
  const std::uint64_t pid = player->id;

  // Edit-mode overlay: built-in Grass / Dirt are open ground.
  const runtime::WorldRect all{-4000.0f, -4000.0f, 4000.0f, 4000.0f};
  std::vector<runtime::OverlayBox> boxes;
  scr.build_collision_overlay(all, &boxes);
  if (count_kind(boxes, runtime::OverlayKind::SolidTile) != 0 ||
      count_kind(boxes, runtime::OverlayKind::DynamicSolid) != 1) {
    return fail("edit overlay on the seed scene");
  }

  // Palette toggle: Grass becomes a hedge. One step, autosaved; undo and
  // redo flip it and scene.json follows.
  const std::size_t undo0 = w.undo_count();
  if (scr.tile_solid("", 4) || !scr.toggle_tile_solid("", 4) ||
      !scr.tile_solid("", 4) || w.undo_count() != undo0 + 1 ||
      w.undo_label() != "Solid: Grass") {
    return fail("solid toggle should be one undo step (\"" + w.undo_label() +
                "\")");
  }
  const std::string hedge =
      "{\"tileset\": \"\", \"solid\": [3, 4, 5, 6, 8, 11, 12, 14, 15]}";
  if (read_text(scene).find(hedge) == std::string::npos) {
    return fail("solid toggle should autosave tile_solidity");
  }
  scr.build_collision_overlay(all, &boxes);
  if (count_kind(boxes, runtime::OverlayKind::SolidTile) != 1) {
    return fail("edit overlay should show the hedge row as one run");
  }
  if (!scr.undo() || scr.tile_solid("", 4) ||
      read_text(scene).find("\"tile_solidity\": []") == std::string::npos) {
    return fail("undo should make Grass walkable again");
  }
  if (!scr.redo() || !scr.tile_solid("", 4) ||
      read_text(scene).find(hedge) == std::string::npos) {
    return fail("redo should make Grass solid again");
  }

  // Collider edits: remove / undo / redo / undo, then a resize.
  const ColliderData rider_box = *w.find(pid)->collider;
  if (!scr.set_collider(pid, std::nullopt, "Remove Collider") ||
      w.find(pid)->collider || !scr.undo() || !w.find(pid)->collider ||
      *w.find(pid)->collider != rider_box || !scr.redo() ||
      w.find(pid)->collider || !scr.undo()) {
    return fail("collider remove should undo / redo");
  }
  ColliderData slim = rider_box;
  slim.offset_x = 4.0f;
  slim.w = 24.0f;
  if (!scr.set_collider(pid, slim, "Edit Collider") ||
      w.undo_label() != "Edit Collider" ||
      read_text(scene).find("\"collider\": {\"x\": 4, \"y\": 0, \"w\": 24, "
                            "\"h\": 48") == std::string::npos ||
      !scr.undo() || *w.find(pid)->collider != rider_box) {
    return fail("collider edit should be one step in scene.json");
  }

  // A gate across the rider's path south.
  const std::uint64_t gid = scr.create_entity("Gate");
  if (Entity2D* g = w.find(gid)) {
    g->x = 64.0f;
    g->y = 120.0f;
    g->w = 32.0f;
    g->h = 30.0f;
  }
  if (gid == 0 ||
      !scr.set_collider(gid, ColliderData{0.0f, 0.0f, 32.0f, 30.0f, true, false},
                        "Add Collider")) {
    return fail("gate trigger");
  }
  const std::string disk_before = read_text(scene);

  // Play: ride south into the hedge. Stops flush on it (bottom at 160),
  // the gate fires once, edits are locked, the overlay is live.
  if (!scr.start_play()) {
    return fail("play with collision");
  }
  for (int i = 0; i < 90; ++i) {
    scr.update_play(1.0 / 60.0, ride(0.0f, 1.0f));
  }
  const runtime::Actor* rider = scr.play_session().world().player(0);
  const std::string trig = scr.last_trigger_text();
  if (!rider || !near(runtime::collider_box(rider->data).y1, 160.0f, 1.0e-3f) ||
      trig.rfind("Player rode into Gate (tick ", 0) != 0 ||
      scr.play_session().world().trigger_event_total() != 1) {
    return fail("play: rider should stop on the hedge and ride into the gate "
                "(\"" + trig + "\")");
  }
  scr.build_collision_overlay(all, &boxes);
  bool gate_lit = false;
  for (const runtime::OverlayBox& b : boxes) {
    gate_lit |= b.kind == runtime::OverlayKind::Trigger && b.active;
  }
  if (!gate_lit || count_kind(boxes, runtime::OverlayKind::SolidTile) != 1) {
    return fail("play overlay should show the hedge and a lit gate");
  }
  if (scr.set_tile_solid("", 1, true) ||
      scr.set_collider(pid, std::nullopt, "Remove Collider")) {
    return fail("collision edits must be locked while playing");
  }
  scr.stop_play();
  if (!scr.tile_solid("", 4) || read_text(scene) != disk_before ||
      !scr.last_trigger_text().empty()) {
    return fail("stop should leave solidity and scene.json alone");
  }
  scr.toggle_collision_overlay();
  if (!scr.show_collision()) {
    return fail("K overlay toggle");
  }
  scr.on_exit();
  fs::remove_all(dir, ec);
  return 0;
}

}  // namespace

int run_collision_smoke() {
  if (check_v4_format() != 0 || check_walls() != 0 ||
      check_riders() != 0 || check_triggers() != 0) {
    return 1;
  }
  std::cout << "[smoke] collision OK (solid tile stops flush, wall slide "
               "keeps parallel motion, 5000 px/s no tunnel through tile or "
               "4 px fence, static crate blocks, dynamic crate shoved, "
               "tile_solidity overrides, riders separate, trigger enter/exit "
               "once each, overlay, v3 -> v4 upgrade + roundtrip)\n";
  if (check_editor() != 0) {
    return 1;
  }
  std::cout << "[smoke] collision editor OK (palette solid toggle = 1 undo "
               "step + scene.json, undo/redo, collider remove/edit undo/redo, "
               "Play stops on the edited wall, gate fires once, K overlay "
               "edit + play, edits locked while riding)\n";
  return 0;
}
