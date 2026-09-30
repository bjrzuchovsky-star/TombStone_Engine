#include "editor/workspace/Workspace2D.h"

#include <algorithm>
#include <cmath>

namespace ts {
namespace tombstone {
namespace editor {

namespace {

constexpr float kMinZoom = 0.15f;
constexpr float kMaxZoom = 8.0f;

}  // namespace

Workspace2D::Workspace2D() {
  reset_defaults();
}

void Workspace2D::reset_defaults() {
  entities_.clear();
  selected_id_.reset();
  next_id_ = 1;
  pan_x_ = 0.0f;
  pan_y_ = 0.0f;
  zoom_ = 1.0f;
  show_grid_ = true;

  Entity2D camera;
  camera.id = alloc_id();
  camera.name = "Camera2D";
  camera.x = 0.0f;
  camera.y = 0.0f;
  camera.w = 48.0f;
  camera.h = 32.0f;
  camera.color[0] = 0.95f;
  camera.color[1] = 0.75f;
  camera.color[2] = 0.25f;
  camera.color[3] = 1.0f;
  camera.layer = 10;
  entities_.push_back(camera);

  Entity2D player;
  player.id = alloc_id();
  player.name = "Player";
  player.x = 64.0f;
  player.y = 64.0f;
  player.w = 32.0f;
  player.h = 48.0f;
  player.color[0] = 0.35f;
  player.color[1] = 0.75f;
  player.color[2] = 0.45f;
  player.color[3] = 1.0f;
  player.layer = 5;
  entities_.push_back(player);

  Entity2D tilemap;
  tilemap.id = alloc_id();
  tilemap.name = "TileMap";
  tilemap.x = 0.0f;
  tilemap.y = 160.0f;
  tilemap.w = 256.0f;
  tilemap.h = 64.0f;
  tilemap.color[0] = 0.45f;
  tilemap.color[1] = 0.45f;
  tilemap.color[2] = 0.55f;
  tilemap.color[3] = 1.0f;
  tilemap.layer = 0;
  entities_.push_back(tilemap);

  selected_id_ = player.id;
}

void Workspace2D::select(std::optional<std::uint64_t> id) {
  if (!id) {
    selected_id_.reset();
    return;
  }
  if (find(*id)) {
    selected_id_ = id;
  }
}

Entity2D* Workspace2D::find(std::uint64_t id) {
  for (auto& e : entities_) {
    if (e.id == id) {
      return &e;
    }
  }
  return nullptr;
}

const Entity2D* Workspace2D::find(std::uint64_t id) const {
  for (const auto& e : entities_) {
    if (e.id == id) {
      return &e;
    }
  }
  return nullptr;
}

Entity2D* Workspace2D::selected() {
  if (!selected_id_) {
    return nullptr;
  }
  return find(*selected_id_);
}

const Entity2D* Workspace2D::selected() const {
  if (!selected_id_) {
    return nullptr;
  }
  return find(*selected_id_);
}

std::uint64_t Workspace2D::create_entity(std::string name) {
  if (name.empty()) {
    name = "Entity";
  }
  Entity2D e;
  e.id = alloc_id();
  e.name = unique_name(entities_, std::move(name));
  e.x = 32.0f + static_cast<float>(entities_.size() % 5) * 24.0f;
  e.y = 32.0f + static_cast<float>(entities_.size() % 4) * 24.0f;
  e.w = 64.0f;
  e.h = 64.0f;
  e.layer = static_cast<int>(entities_.size());
  entities_.push_back(e);
  selected_id_ = e.id;
  return e.id;
}

bool Workspace2D::rename_entity(std::uint64_t id, std::string name) {
  Entity2D* e = find(id);
  if (!e || name.empty()) {
    return false;
  }
  e->name = std::move(name);
  return true;
}

bool Workspace2D::delete_entity(std::uint64_t id) {
  const auto it =
      std::find_if(entities_.begin(), entities_.end(),
                   [id](const Entity2D& e) { return e.id == id; });
  if (it == entities_.end()) {
    return false;
  }
  entities_.erase(it);
  if (selected_id_ && *selected_id_ == id) {
    selected_id_.reset();
    if (!entities_.empty()) {
      selected_id_ = entities_.front().id;
    }
  }
  return true;
}

void Workspace2D::set_pan(float x, float y) {
  pan_x_ = x;
  pan_y_ = y;
}

void Workspace2D::add_pan(float dx, float dy) {
  pan_x_ += dx;
  pan_y_ += dy;
}

void Workspace2D::set_zoom(float z) {
  zoom_ = std::clamp(z, kMinZoom, kMaxZoom);
}

void Workspace2D::adjust_zoom(float factor, float anchor_screen_x,
                              float anchor_screen_y, float viewport_w,
                              float viewport_h) {
  const float old_zoom = zoom_;
  const float new_zoom = std::clamp(old_zoom * factor, kMinZoom, kMaxZoom);
  if (std::abs(new_zoom - old_zoom) < 1e-6f) {
    return;
  }

  // Keep world point under the cursor stable across zoom.
  const float world_x =
      (anchor_screen_x - viewport_w * 0.5f) / old_zoom + pan_x_;
  const float world_y =
      (anchor_screen_y - viewport_h * 0.5f) / old_zoom + pan_y_;
  zoom_ = new_zoom;
  pan_x_ = world_x - (anchor_screen_x - viewport_w * 0.5f) / new_zoom;
  pan_y_ = world_y - (anchor_screen_y - viewport_h * 0.5f) / new_zoom;
}

std::vector<std::size_t> Workspace2D::sorted_draw_order() const {
  std::vector<std::size_t> order(entities_.size());
  for (std::size_t i = 0; i < entities_.size(); ++i) {
    order[i] = i;
  }
  std::stable_sort(order.begin(), order.end(),
                   [this](std::size_t a, std::size_t b) {
                     if (entities_[a].layer != entities_[b].layer) {
                       return entities_[a].layer < entities_[b].layer;
                     }
                     return entities_[a].id < entities_[b].id;
                   });
  return order;
}

std::uint64_t Workspace2D::alloc_id() {
  return next_id_++;
}

std::string Workspace2D::unique_name(const std::vector<Entity2D>& existing,
                                     std::string base) {
  bool clash = false;
  for (const auto& e : existing) {
    if (e.name == base) {
      clash = true;
      break;
    }
  }
  if (!clash) {
    return base;
  }
  for (int i = 2; i < 10000; ++i) {
    const std::string candidate = base + " " + std::to_string(i);
    bool found = false;
    for (const auto& e : existing) {
      if (e.name == candidate) {
        found = true;
        break;
      }
    }
    if (!found) {
      return candidate;
    }
  }
  return base + " x";
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
