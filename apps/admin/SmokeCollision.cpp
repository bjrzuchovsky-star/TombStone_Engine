#include "SmokeCollision.h"

#include "scene/SceneData.h"
#include "scene/SceneJson.h"
#include "scene/TileMap.h"

#include <iostream>
#include <string>

namespace {

using namespace ts::tombstone;

int fail(const std::string& what) {
  std::cerr << "[smoke] collision FAILED: " << what << '\n';
  return 1;
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

}  // namespace

int run_collision_smoke() {
  if (check_v4_format() != 0) {
    return 1;
  }
  std::cout << "[smoke] collision OK (scene.json v4 colliders + tile "
               "solidity roundtrip, built-in solid defaults, v3 -> v4 "
               "rider collider upgrade)\n";
  return 0;
}
