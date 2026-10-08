#include "scene/SceneData.h"

#include <algorithm>
#include <numeric>

namespace ts {
namespace tombstone {

bool operator==(const Entity2D& a, const Entity2D& b) {
  return a.id == b.id && a.name == b.name && a.x == b.x && a.y == b.y &&
         a.w == b.w && a.h == b.h && a.color[0] == b.color[0] &&
         a.color[1] == b.color[1] && a.color[2] == b.color[2] &&
         a.color[3] == b.color[3] && a.layer == b.layer &&
         a.tilemap == b.tilemap && a.sprite == b.sprite &&
         a.player == b.player && a.camera == b.camera && a.spawn == b.spawn;
}

void sync_tilemap_extent(Entity2D& e) {
  if (!e.tilemap) {
    return;
  }
  e.tilemap->normalize();
  e.w = static_cast<float>(e.tilemap->cols * e.tilemap->tile_size);
  e.h = static_cast<float>(e.tilemap->rows * e.tilemap->tile_size);
}

void normalize_components(Entity2D& e) {
  if (e.player) {
    e.player->slot = std::clamp(e.player->slot, 0, kMaxPlayers - 1);
    if (!(e.player->speed >= PlayerControllerData::kMinSpeed)) {
      e.player->speed = PlayerControllerData::kMinSpeed;  // also catches NaN
    }
    e.player->speed =
        std::min(e.player->speed, PlayerControllerData::kMaxSpeed);
  }
  if (e.camera) {
    Camera2DData& c = *e.camera;
    if (!(c.smoothing >= 0.0f)) {
      c.smoothing = 0.0f;
    }
    c.smoothing = std::min(c.smoothing, 5.0f);
    if (!(c.zoom >= 0.05f)) {
      c.zoom = 0.05f;
    }
    c.zoom = std::min(c.zoom, 16.0f);
    c.bounds_w = std::max(1.0f, c.bounds_w);
    c.bounds_h = std::max(1.0f, c.bounds_h);
  }
  if (e.spawn) {
    e.spawn->slot = std::clamp(e.spawn->slot, 0, kMaxPlayers - 1);
  }
}

const Entity2D* find_entity(const std::vector<Entity2D>& entities,
                            std::uint64_t id) {
  for (const Entity2D& e : entities) {
    if (e.id == id) {
      return &e;
    }
  }
  return nullptr;
}

const Entity2D* find_entity_named(const std::vector<Entity2D>& entities,
                                  const std::string& name) {
  for (const Entity2D& e : entities) {
    if (e.name == name) {
      return &e;
    }
  }
  return nullptr;
}

std::vector<std::size_t> draw_order(const std::vector<Entity2D>& entities) {
  std::vector<std::size_t> order(entities.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(order.begin(), order.end(),
                   [&](std::size_t a, std::size_t b) {
                     if (entities[a].layer != entities[b].layer) {
                       return entities[a].layer < entities[b].layer;
                     }
                     return entities[a].id < entities[b].id;
                   });
  return order;
}

}  // namespace tombstone
}  // namespace ts
