#pragma once

// Authored scene data: what scene.json stores and what both the editor
// (Workspace2D) and the runtime (runtime::World) start from. Plain structs,
// no UI and no graphics API.

#include "scene/TileMap.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {

// Local player slots. Slot 0 is the keyboard rider; 1-3 are reserved for
// gamepads now and networked riders later.
inline constexpr int kMaxPlayers = 4;

// Moves the entity from a player's input (scene.json "player").
struct PlayerControllerData {
  static constexpr float kMinSpeed = 1.0f;
  static constexpr float kMaxSpeed = 5000.0f;

  int slot = 0;          // 0..kMaxPlayers-1
  float speed = 160.0f;  // world px per second at full stick

  bool operator==(const PlayerControllerData& o) const = default;
};

// Follow camera (scene.json "camera"). The camera looks at the centre of
// `target` (0 = stay put at the entity's own centre).
struct Camera2DData {
  std::uint64_t target = 0;  // entity id to follow
  float smoothing = 0.15f;   // catch-up time in seconds, 0 = locked on
  float zoom = 1.0f;         // screen px per world px
  // Optional world-space box the view may not leave.
  bool use_bounds = false;
  float bounds_x = 0.0f;
  float bounds_y = 0.0f;
  float bounds_w = 1024.0f;
  float bounds_h = 768.0f;

  bool operator==(const Camera2DData& o) const = default;
};

// Where player `slot` starts when play begins (scene.json "spawn").
struct SpawnPointData {
  int slot = 0;

  bool operator==(const SpawnPointData& o) const = default;
};

// One entity in a flat scene under the conceptual root. Transform is the
// x/y/w/h rect (y grows down); everything else is an optional component.
struct Entity2D {
  std::uint64_t id = 0;
  std::string name;
  float x = 0.0f;
  float y = 0.0f;
  float w = 64.0f;
  float h = 64.0f;
  float color[4] = {0.35f, 0.65f, 0.95f, 1.0f};  // RGBA tint
  int layer = 0;                                  // z-order (higher draws later)
  // A TileMap entity's w/h follow its grid; a sprite draws a textured quad
  // tinted by `color` (falls back to the rect).
  std::optional<TileMapData> tilemap;
  std::optional<SpriteData> sprite;
  // Gameplay components (scene.json v3).
  std::optional<PlayerControllerData> player;
  std::optional<Camera2DData> camera;
  std::optional<SpawnPointData> spawn;

  float center_x() const { return x + w * 0.5f; }
  float center_y() const { return y + h * 0.5f; }
  bool has_gameplay() const { return player || camera || spawn; }
};

bool operator==(const Entity2D& a, const Entity2D& b);
inline bool operator!=(const Entity2D& a, const Entity2D& b) {
  return !(a == b);
}

// Keep w/h equal to the TileMap grid extent (no-op without a tilemap).
void sync_tilemap_extent(Entity2D& e);

// Clamp component fields to their valid ranges (slots, speed, zoom...).
void normalize_components(Entity2D& e);

// Lookup helpers over a flat entity list.
const Entity2D* find_entity(const std::vector<Entity2D>& entities,
                            std::uint64_t id);
const Entity2D* find_entity_named(const std::vector<Entity2D>& entities,
                                  const std::string& name);

// Indices sorted by layer ascending, stable by id (the shared draw order).
std::vector<std::size_t> draw_order(const std::vector<Entity2D>& entities);

}  // namespace tombstone
}  // namespace ts
