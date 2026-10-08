#include "SmokeRuntime.h"

#include "editor/workspace/SceneIO.h"
#include "editor/workspace/Workspace2D.h"
#include "runtime/PlaySession.h"
#include "runtime/World.h"
#include "scene/SceneJson.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace ts::tombstone;

bool near(float a, float b, float eps = 0.01f) {
  return std::fabs(a - b) <= eps;
}

int fail(const std::string& what) {
  std::cerr << "[smoke] runtime FAILED: " << what << '\n';
  return 1;
}

std::string read_file(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

runtime::InputFrame move(float x, float y, int slot = 0) {
  runtime::InputFrame f;
  f.slot(slot).move_x = x;
  f.slot(slot).move_y = y;
  f.slot(slot).connected = true;
  return f;
}

// Seeded editor scene: Camera2D (follows Player), Player (slot 0), TileMap.
std::vector<Entity2D> seed_entities() {
  editor::Workspace2D w;
  w.reset_defaults();
  return w.entities();
}

std::uint64_t id_named(const std::vector<Entity2D>& es, const char* name) {
  const Entity2D* e = find_entity_named(es, name);
  return e ? e->id : 0;
}

int check_world() {
  const std::vector<Entity2D> seed = seed_entities();
  const std::uint64_t player_id = id_named(seed, "Player");
  const std::uint64_t cam_id = id_named(seed, "Camera2D");
  const Entity2D* pe = find_entity(seed, player_id);
  const Entity2D* ce = find_entity(seed, cam_id);
  if (!pe || !pe->player || pe->player->slot != 0 || !ce || !ce->camera ||
      ce->camera->target != player_id) {
    return fail("seed scene should wire Player (slot 0) + Camera2D follow");
  }
  const float speed = pe->player->speed;
  const float start_x = pe->x;
  const float start_y = pe->y;

  runtime::World world;
  std::string err;
  if (!world.build(seed, "", &err) || world.player_count() != 1) {
    return fail("build world: " + err);
  }
  // Camera starts on the rider, not a smoothing-length pan away.
  runtime::CameraView cam = world.camera();
  if (!near(cam.x, pe->center_x()) || !near(cam.y, pe->center_y()) ||
      world.camera_entity() != cam_id || world.camera_target() != player_id) {
    return fail("camera should start centred on the player");
  }
  // 60 ticks (one second) of full right: speed px.
  for (int i = 0; i < runtime::kTickRate; ++i) {
    world.step(move(1.0f, 0.0f));
  }
  const runtime::Actor* p = world.player(0);
  if (world.tick() != 60 || !p || !near(p->data.x - start_x, speed, 0.05f) ||
      !near(p->data.y, start_y) || p->facing != 1 || !near(p->vx, speed)) {
    return fail("player should move speed px in 60 ticks (x=" +
                std::to_string(p ? p->data.x : 0.0f) + ")");
  }
  // Follow camera trails by about speed * smoothing, then settles.
  cam = world.camera();
  const float lag = p->data.center_x() - cam.x;
  if (!(lag > 1.0f && lag < speed * ce->camera->smoothing * 1.5f) ||
      !near(cam.y, p->data.center_y(), 0.05f)) {
    return fail("camera should trail the player (lag " + std::to_string(lag) +
                ")");
  }
  for (int i = 0; i < runtime::kTickRate; ++i) {
    world.step(runtime::InputFrame{});
  }
  cam = world.camera();
  if (!near(cam.x, p->data.center_x(), 0.5f) ||
      !near(p->data.x - start_x, speed, 0.05f)) {
    return fail("camera should settle on a stopped player");
  }
  // The camera entity rides along with the view centre.
  const runtime::Actor* ca = world.find(cam_id);
  if (!ca || !near(ca->data.center_x(), cam.x, 0.01f)) {
    return fail("camera entity should track the view");
  }
  // Diagonals are clamped to unit length; other slots are ignored.
  const float before_x = p->data.x;
  const float before_y = p->data.y;
  for (int i = 0; i < runtime::kTickRate; ++i) {
    runtime::InputFrame f = move(1.0f, 1.0f);
    f.slot(1).move_x = -1.0f;  // nobody rides slot 1
    world.step(f);
  }
  const float dx = p->data.x - before_x;
  const float dy = p->data.y - before_y;
  if (!near(std::sqrt(dx * dx + dy * dy), speed, 0.1f) || !near(dx, dy)) {
    return fail("diagonal should be clamped to speed");
  }
  // Interpolation: alpha 0 is the previous tick, 1 the latest.
  world.step(move(-1.0f, 0.0f));
  if (!near(p->prev_x - p->data.x, speed / runtime::kTickRate, 0.01f) ||
      p->facing != -1) {
    return fail("prev position / facing");
  }

  // Spawn point: player 0 starts centred on it.
  {
    std::vector<Entity2D> es = seed;
    Entity2D spawn;
    spawn.id = 99;
    spawn.name = "Spawn";
    spawn.x = 400.0f;
    spawn.y = 300.0f;
    spawn.w = 32.0f;
    spawn.h = 32.0f;
    spawn.spawn = SpawnPointData{};
    es.push_back(spawn);
    runtime::World sw;
    sw.build(es);
    const runtime::Actor* sp = sw.player(0);
    if (!sp || !near(sp->data.center_x(), 416.0f) ||
        !near(sp->data.center_y(), 316.0f) ||
        !near(sw.camera().x, 416.0f)) {
      return fail("player should start on its SpawnPoint");
    }
  }
  // Bounds: a 1280x720 view inside a 1280x720 box never moves.
  {
    std::vector<Entity2D> es = seed;
    for (Entity2D& e : es) {
      if (e.camera) {
        e.camera->use_bounds = true;
        e.camera->bounds_x = 0.0f;
        e.camera->bounds_y = 0.0f;
        e.camera->bounds_w = 1280.0f;
        e.camera->bounds_h = 720.0f;
        e.camera->smoothing = 0.0f;
      }
    }
    runtime::World bw;
    bw.set_view_size(1280.0f, 720.0f);
    bw.build(es);
    for (int i = 0; i < 120; ++i) {
      bw.step(move(1.0f, 1.0f));
    }
    const runtime::CameraView bc = bw.camera();
    if (!near(bc.x, 640.0f) || !near(bc.y, 360.0f)) {
      return fail("camera bounds clamp");
    }
  }
  // Draw list: tilemap tiles (built-in: base + lip + lit), the player rect,
  // no camera / spawn markers; a missing sprite is flagged.
  {
    std::vector<Entity2D> es = seed;
    Entity2D ghost;
    ghost.id = 77;
    ghost.name = "Ghost";
    ghost.x = 300.0f;
    ghost.sprite = SpriteData{};
    ghost.sprite->path = "assets/nope.png";
    es.push_back(ghost);
    runtime::World dw;
    dw.build(es);
    std::vector<runtime::DrawQuad> quads;
    const runtime::WorldRect view{-2000.0f, -2000.0f, 2000.0f, 2000.0f};
    dw.build_draw_list(view, 1.0f, runtime::ImageSizeFn(), &quads);
    int tiles = 0;
    int players = 0;
    int cams = 0;
    int missing = 0;
    for (const runtime::DrawQuad& q : quads) {
      const Entity2D* src = find_entity(es, q.entity);
      if (src && src->tilemap) ++tiles;
      if (q.entity == player_id) ++players;
      if (q.entity == cam_id) ++cams;
      if (q.missing && q.entity == 77) ++missing;
    }
    if (tiles != 16 * 3 || players != 1 || cams != 0 || missing != 1) {
      return fail("draw list (tiles " + std::to_string(tiles) + ")");
    }
    // Culling: nothing when looking far away.
    dw.build_draw_list({5000.0f, 5000.0f, 6000.0f, 6000.0f}, 1.0f,
                       runtime::ImageSizeFn(), &quads);
    if (!quads.empty()) {
      return fail("draw list culling");
    }
  }
  return 0;
}

int check_session() {
  const std::vector<Entity2D> seed = seed_entities();
  const std::vector<Entity2D> pristine = seed;
  runtime::PlaySession s;
  if (s.active() || !s.start(seed, "") || !s.playing()) {
    return fail("session start");
  }
  // Fixed step: 250 frames of 4 ms = 1 s = 60 ticks (whatever the frame
  // rate), positions advance by whole ticks only.
  int ran = 0;
  for (int i = 0; i < 250; ++i) {
    ran += s.update(0.004, move(1.0f, 0.0f));
  }
  const runtime::Actor* p = s.world().player(0);
  const float speed = p->data.player->speed;
  const float x0 = find_entity(seed, p->data.id)->x;
  if (ran < 59 || ran > 60 || s.tick() != static_cast<std::uint64_t>(ran) ||
      !near(p->data.x - x0,
            speed * static_cast<float>(ran) / runtime::kTickRate, 0.05f)) {
    return fail("fixed step accumulator (" + std::to_string(ran) + " ticks)");
  }
  // A long hitch is capped instead of spiralling.
  if (s.update(5.0, runtime::InputFrame{}) > runtime::PlaySession::kMaxCatchUpTicks) {
    return fail("catch-up cap");
  }
  // Pause: no simulation; step: exactly one tick.
  s.pause();
  const std::uint64_t t = s.tick();
  const float px = s.world().player(0)->data.x;
  for (int i = 0; i < 30; ++i) {
    s.update(1.0 / 60.0, move(1.0f, 0.0f));
  }
  if (!s.paused() || s.tick() != t || s.world().player(0)->data.x != px) {
    return fail("pause should stop the simulation");
  }
  if (!s.step_once(move(1.0f, 0.0f)) || s.tick() != t + 1 ||
      !near(s.world().player(0)->data.x - px, speed / runtime::kTickRate)) {
    return fail("step should advance exactly one tick");
  }
  s.resume();
  if (!s.playing() || s.step_once(runtime::InputFrame{})) {
    return fail("step only while paused");
  }
  s.stop();
  if (s.active() || !s.world().empty() || s.tick() != 0 || seed != pristine) {
    return fail("stop should drop the world and leave the scene alone");
  }
  return 0;
}

int check_scene_versions() {
  // A v2 file (pre-gameplay) gains the seed wiring on load.
  const std::string v2 =
      "{\n  \"version\": 2,\n  \"entities\": [\n"
      "    {\"id\": 1, \"name\": \"Camera2D\", \"x\": 0, \"y\": 0, \"w\": 48, "
      "\"h\": 32, \"layer\": 10},\n"
      "    {\"id\": 2, \"name\": \"Player\", \"x\": 64, \"y\": 64, \"w\": 32, "
      "\"h\": 48, \"layer\": 5},\n"
      "    {\"id\": 3, \"name\": \"TileMap\", \"x\": 0, \"y\": 160, \"layer\": "
      "0,\n      \"tilemap\": {\"cols\": 2, \"rows\": 1, \"tile_size\": 32, "
      "\"tileset\": \"\", \"encoding\": \"rle\", \"data\": \"4,1\"}}\n"
      "  ]\n}\n";
  scene_json::SceneDoc doc;
  std::string err;
  if (!scene_json::parse(v2, &doc, &err) || doc.version != 2 ||
      doc.entities.size() != 3) {
    return fail("v2 parse: " + err);
  }
  const Entity2D* cam = find_entity(doc.entities, 1);
  const Entity2D* pl = find_entity(doc.entities, 2);
  const Entity2D* tm = find_entity(doc.entities, 3);
  if (!pl || !pl->player || pl->player->slot != 0 || !cam || !cam->camera ||
      cam->camera->target != 2 || !tm || !tm->tilemap || tm->player ||
      tm->tilemap->at(0, 0) != 4 || tm->w != 64.0f) {
    return fail("v2 -> v3 upgrade");
  }
  const std::string v3 = scene_json::write(doc);
  if (v3.find("\"version\": 3") == std::string::npos ||
      v3.find("\"player\": {\"slot\": 0, \"speed\": 160}") ==
          std::string::npos ||
      v3.find("\"camera\": {\"target\": 2, \"smoothing\": 0.15") ==
          std::string::npos) {
    return fail("v3 write:\n" + v3);
  }
  // v3 round trip keeps every component field bit-exact, and a v3 file is
  // never re-upgraded (removing the player sticks).
  scene_json::SceneDoc back;
  if (!scene_json::parse(v3, &back, &err) || back.entities != doc.entities ||
      scene_json::write(back) != v3) {
    return fail("v3 roundtrip: " + err);
  }
  Entity2D& rider = back.entities[1];
  rider.player->slot = 3;
  rider.player->speed = 212.5f;
  back.entities[0].camera->use_bounds = true;
  back.entities[0].camera->bounds_w = 3000.25f;
  back.entities[0].camera->zoom = 2.0f;
  back.entities[0].camera->smoothing = 0.0f;
  Entity2D spawn;
  spawn.id = 9;
  spawn.name = "Spawn 2";
  spawn.spawn = SpawnPointData{2};
  back.entities.push_back(spawn);
  back.entities[2].player.reset();
  scene_json::SceneDoc again;
  if (!scene_json::parse(scene_json::write(back), &again, &err) ||
      again.entities != back.entities) {
    return fail("v3 component roundtrip: " + err);
  }
  back.entities[1].player.reset();
  if (!scene_json::parse(scene_json::write(back), &again, &err) ||
      again.entities[1].player) {
    return fail("v3 files must not be re-upgraded");
  }
  // Junk components are rejected with a reason.
  if (scene_json::parse("{\"entities\": [{\"id\": 1, \"name\": \"A\", "
                        "\"player\": {\"slot\": }}]}",
                        &again, &err)) {
    return fail("bad player object should not load");
  }
  // World::load_project reads scene.json from disk.
  namespace fs = std::filesystem;
  const fs::path dir = fs::path("TombStoneProjects") / "_runtime_smoke";
  fs::create_directories(dir);
  if (!scene_json::save_file(
          scene_json::scene_path_for_project(dir.string()), doc, &err)) {
    return fail("save: " + err);
  }
  runtime::World w;
  if (!w.load_project(dir.string(), &err) || w.player_count() != 1 ||
      w.project_dir() != dir.string()) {
    return fail("load_project: " + err);
  }
  if (runtime::resolve_asset(dir.string(), "assets/a.png") !=
      (dir / "assets/a.png").string()) {
    return fail("resolve_asset");
  }
  return 0;
}

}  // namespace

int run_runtime_smoke() {
  if (check_world() != 0 || check_session() != 0 ||
      check_scene_versions() != 0) {
    return 1;
  }
  std::cout << "[smoke] runtime OK (60 Hz fixed step, player px/s, diagonal "
               "clamp, spawn point, follow camera + bounds, draw list, "
               "pause/step/stop, v2 -> v3 upgrade + roundtrip)\n";
  return 0;
}
