#pragma once

// Collision shapes shared by the runtime World, the editor's collider
// overlay (K) and ts_game: axis-aligned boxes in world space. No UI, no GL.

#include "scene/SceneData.h"
#include "scene/TileMap.h"

#include <cstdint>
#include <vector>

namespace ts {
namespace tombstone {
namespace runtime {

struct WorldRect;

struct Aabb {
  float x0 = 0.0f;
  float y0 = 0.0f;
  float x1 = 0.0f;
  float y1 = 0.0f;
};

// Touching edges do not overlap (riders stop flush against a wall and then
// slide along it), and neither do overlaps thinner than kContactSlop.
inline constexpr float kContactSlop = 1.0e-3f;
bool overlaps(const Aabb& a, const Aabb& b);

// The entity's collider box with its top-left corner at (x, y). Without a
// collider: the entity rect.
Aabb collider_box(const Entity2D& e, float x, float y);
inline Aabb collider_box(const Entity2D& e) {
  return collider_box(e, e.x, e.y);
}

// What the collision overlay (editor K, ts_game K) outlines.
enum class OverlayKind { SolidTile, StaticSolid, DynamicSolid, Trigger };

struct OverlayBox {
  float x0 = 0.0f;
  float y0 = 0.0f;
  float x1 = 0.0f;
  float y1 = 0.0f;
  OverlayKind kind = OverlayKind::SolidTile;
  bool active = false;  // trigger with somebody inside (play only)
  std::uint64_t entity = 0;
};

// Overlay for one entity drawn with its top-left corner at (x, y): its
// collider, and for a TileMap its solid tiles (each row merged into runs).
// Culled to `view`.
void append_overlay(const Entity2D& e, float x, float y,
                    const TileSolidity& solidity, const WorldRect& view,
                    std::vector<OverlayBox>* out);

}  // namespace runtime
}  // namespace tombstone
}  // namespace ts
