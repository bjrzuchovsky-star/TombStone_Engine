// Collision tools: the K overlay (edit and play), the Inspector's Collision
// section and the Tile Palette's solid toggle. Every edit is one undo step
// and autosaves scene.json (v4 "collider" / "tile_solidity").
#include "editor/screens/Editor2DScreen.h"

#include "editor/ui/Theme.h"
#include "runtime/Collision.h"
#include "runtime/World.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

namespace {

// "Stone" for the built-in palette, "#7" for an image tileset.
std::string tile_label(const std::string& tileset, int tile_id) {
  if (tileset.empty()) {
    return builtin_tile(tile_id).name;
  }
  return "#" + std::to_string(tile_id);
}

// Walls red, static props copper, riders green, triggers gold.
ImVec4 overlay_color(runtime::OverlayKind kind) {
  switch (kind) {
    case runtime::OverlayKind::SolidTile:
      return theme::Danger();
    case runtime::OverlayKind::StaticSolid:
      return theme::Copper();
    case runtime::OverlayKind::DynamicSolid:
      return theme::Success();
    case runtime::OverlayKind::Trigger:
      return theme::Warning();
  }
  return theme::Accent();
}

float overlay_fill(const runtime::OverlayBox& b) {
  switch (b.kind) {
    case runtime::OverlayKind::SolidTile:
      return 0.30f;
    case runtime::OverlayKind::StaticSolid:
      return 0.22f;
    case runtime::OverlayKind::DynamicSolid:
      return 0.12f;
    case runtime::OverlayKind::Trigger:
      return b.active ? 0.40f : 0.10f;
  }
  return 0.2f;
}

}  // namespace

void draw_solid_marker(ImDrawList* draw, const ImVec2& p0, const ImVec2& p1) {
  // Red corner wedge + inner rule: "riders can't pass".
  const float s = std::min(12.0f, (p1.x - p0.x) * 0.4f);
  draw->AddTriangleFilled(p0, ImVec2(p0.x + s, p0.y), ImVec2(p0.x, p0.y + s),
                          theme::U32(theme::Danger()));
  draw->AddRect(ImVec2(p0.x + 1.0f, p0.y + 1.0f),
                ImVec2(p1.x - 1.0f, p1.y - 1.0f),
                theme::U32(theme::Danger(), 0.85f), 0.0f, 0, 1.5f);
}

// --- Tile solidity --------------------------------------------------------------

bool Editor2DScreen::tile_solid(const std::string& tileset, int tile_id) const {
  return workspace_.tile_solidity().solid(tileset, tile_id);
}

bool Editor2DScreen::set_tile_solid(const std::string& tileset, int tile_id,
                                    bool solid) {
  if (is_playing() || tile_id <= 0) {
    return false;
  }
  Workspace2D::Snapshot before = prepare_edit();
  if (!workspace_.set_tile_solid(tileset, tile_id, solid)) {
    return false;
  }
  const std::string name = tile_label(tileset, tile_id);
  return commit_discrete(std::move(before),
                         (solid ? "Solid: " : "Walkable: ") + name,
                         solid ? name + " blocks riders now"
                               : name + " is open ground now");
}

bool Editor2DScreen::toggle_tile_solid(const std::string& tileset,
                                       int tile_id) {
  return set_tile_solid(tileset, tile_id, !tile_solid(tileset, tile_id));
}

// --- Colliders --------------------------------------------------------------------

bool Editor2DScreen::set_collider(std::uint64_t id,
                                  std::optional<ColliderData> collider,
                                  const std::string& label) {
  if (is_playing() || !workspace_.find(id)) {
    return false;
  }
  Workspace2D::Snapshot before = prepare_edit();
  if (!workspace_.set_collider(id, std::move(collider))) {
    return false;
  }
  return commit_discrete(std::move(before), label, "");
}

// --- Overlay (K) --------------------------------------------------------------------

void Editor2DScreen::set_show_collision(bool on) { show_collision_ = on; }

void Editor2DScreen::toggle_collision_overlay() {
  show_collision_ = !show_collision_;
  note(show_collision_ ? "Collision overlay: red walls, copper props, green "
                         "riders, gold triggers. K hides it."
                       : "Collision overlay off.");
}

void Editor2DScreen::build_collision_overlay(
    const runtime::WorldRect& view,
    std::vector<runtime::OverlayBox>* out) const {
  out->clear();
  if (is_playing()) {
    play_.world().build_overlay(view, play_.alpha(), out);
    return;
  }
  for (std::size_t idx : workspace_.sorted_draw_order()) {
    const Entity2D& e = workspace_.entities()[idx];
    runtime::append_overlay(e, e.x, e.y, workspace_.tile_solidity(), view,
                            out);
  }
}

std::string Editor2DScreen::last_trigger_text() const {
  if (!is_playing()) {
    return {};
  }
  const std::optional<runtime::TriggerEvent>& ev =
      play_.world().last_trigger_event();
  if (!ev) {
    return {};
  }
  return play_.world().describe(*ev) + " (tick " + std::to_string(ev->tick) +
         ")";
}

void Editor2DScreen::handle_collision_hotkey() {
  const ImGuiIO& io = ImGui::GetIO();
  if (io.WantTextInput || renaming_ || io.KeyCtrl || io.KeyAlt ||
      io.KeySuper) {
    return;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_K, false)) {
    toggle_collision_overlay();
  }
}

void Editor2DScreen::draw_collision_overlay(ImDrawList* draw,
                                            const ImVec2& canvas_pos,
                                            const ImVec2& canvas_size,
                                            float center_x, float center_y,
                                            float zoom) {
  if (!show_collision_) {
    return;
  }
  zoom = zoom > 0.0f ? zoom : 1.0f;
  auto to_screen = [&](float wx, float wy) {
    return ImVec2(canvas_pos.x + canvas_size.x * 0.5f + (wx - center_x) * zoom,
                  canvas_pos.y + canvas_size.y * 0.5f + (wy - center_y) * zoom);
  };
  const float hw = canvas_size.x * 0.5f / zoom;
  const float hh = canvas_size.y * 0.5f / zoom;
  const runtime::WorldRect view{center_x - hw, center_y - hh, center_x + hw,
                                center_y + hh};
  build_collision_overlay(view, &overlay_boxes_);
  const ImVec2 canvas_end(canvas_pos.x + canvas_size.x,
                          canvas_pos.y + canvas_size.y);
  draw->PushClipRect(canvas_pos, canvas_end, true);
  for (const runtime::OverlayBox& b : overlay_boxes_) {
    const ImVec4 col = overlay_color(b.kind);
    const ImVec2 p0 = to_screen(b.x0, b.y0);
    const ImVec2 p1 = to_screen(b.x1, b.y1);
    draw->AddRectFilled(p0, p1, theme::U32(col, overlay_fill(b)));
    const float thick = b.kind == runtime::OverlayKind::SolidTile ? 1.5f : 2.0f;
    draw->AddRect(p0, p1, theme::U32(col, 0.95f), 0.0f, 0, thick);
    if (b.kind == runtime::OverlayKind::Trigger) {
      // Crossed box: walk-through, never blocks.
      draw->AddLine(p0, p1, theme::U32(col, 0.55f), 1.0f);
      draw->AddLine(ImVec2(p0.x, p1.y), ImVec2(p1.x, p0.y),
                    theme::U32(col, 0.55f), 1.0f);
    }
  }

  // Legend + last trigger event, bottom-left.
  struct Key {
    runtime::OverlayKind kind;
    const char* label;
  };
  static const Key kKeys[] = {
      {runtime::OverlayKind::SolidTile, "wall tiles"},
      {runtime::OverlayKind::StaticSolid, "static"},
      {runtime::OverlayKind::DynamicSolid, "dynamic"},
      {runtime::OverlayKind::Trigger, "trigger"},
  };
  const float line = ImGui::GetTextLineHeight();
  const std::string trig = last_trigger_text();
  const float rows = trig.empty() ? 1.0f : 2.0f;
  float width = ImGui::CalcTextSize("COLLISION (K)").x + 12.0f;
  for (const Key& k : kKeys) {
    width += line + 4.0f + ImGui::CalcTextSize(k.label).x + 10.0f;
  }
  if (!trig.empty()) {
    width = std::max(width, ImGui::CalcTextSize(trig.c_str()).x + 30.0f);
  }
  const ImVec2 b0(canvas_pos.x + 8.0f,
                  canvas_end.y - 8.0f - rows * (line + 4.0f) - 6.0f);
  const ImVec2 b1(b0.x + width + 12.0f, canvas_end.y - 8.0f);
  draw->AddRectFilled(b0, b1, theme::U32(theme::Charcoal(), 0.85f), 3.0f);
  float x = b0.x + 7.0f;
  const float y = b0.y + 5.0f;
  draw->AddText(ImVec2(x, y), theme::U32(theme::Accent()), "COLLISION (K)");
  x += ImGui::CalcTextSize("COLLISION (K)").x + 12.0f;
  for (const Key& k : kKeys) {
    const ImVec4 col = overlay_color(k.kind);
    draw->AddRectFilled(ImVec2(x, y + 2.0f), ImVec2(x + line - 4.0f, y + line - 2.0f),
                        theme::U32(col, 0.6f));
    draw->AddRect(ImVec2(x, y + 2.0f), ImVec2(x + line - 4.0f, y + line - 2.0f),
                  theme::U32(col));
    x += line;
    draw->AddText(ImVec2(x, y), theme::U32(theme::Text()), k.label);
    x += ImGui::CalcTextSize(k.label).x + 10.0f;
  }
  if (!trig.empty()) {
    draw->AddText(ImVec2(b0.x + 7.0f, y + line + 4.0f),
                  theme::U32(theme::Warning()), trig.c_str());
  }
  draw->PopClipRect();
}

// --- Inspector: Collision -----------------------------------------------------------

void Editor2DScreen::draw_inspector_collider(std::uint64_t id) {
  const Entity2D* e = workspace_.find(id);
  if (!e || e->tilemap) {
    return;  // TileMaps block through their solid tiles instead
  }
  ImGui::SeparatorText("Collision");
  ImGui::PushID("collider");
  if (!e->collider) {
    ImGui::TextDisabled("No collider: rides through everything.");
    if (theme::SecondaryButton("+ Collider", ImVec2(120, 0))) {
      ColliderData c = default_collider(*e);
      set_collider(id, c, "Add Collider");
    }
    ImGui::PopID();
    return;
  }
  ColliderData c = *e->collider;
  ImGui::TextColored(theme::Accent(), "Collider (AABB)");
  ImGui::TextDisabled("Type");
  ImGui::SameLine(64.0f);
  if (ImGui::RadioButton("Solid", !c.trigger) && c.trigger) {
    c.trigger = false;
    set_collider(id, c, "Collider: Solid");
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Trigger", c.trigger) && !c.trigger) {
    c.trigger = true;
    set_collider(id, c, "Collider: Trigger");
  }
  ImGui::TextDisabled("Body");
  ImGui::SameLine(64.0f);
  if (ImGui::RadioButton("Static", !c.dynamic) && c.dynamic) {
    c.dynamic = false;
    set_collider(id, c, "Collider: Static");
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Dynamic", c.dynamic) && !c.dynamic) {
    c.dynamic = true;
    set_collider(id, c, "Collider: Dynamic");
  }
  if (Entity2D* t = workspace_.find(id); t && t->collider) {
    float off[2] = {t->collider->offset_x, t->collider->offset_y};
    if (ImGui::DragFloat2("Offset", off, 0.5f, -ColliderData::kMaxSize,
                          ColliderData::kMaxSize, "%.1f")) {
      t->collider->offset_x = off[0];
      t->collider->offset_y = off[1];
      mark_dirty();
    }
    track_inspector_item("Edit Collider Offset");
  }
  if (Entity2D* t = workspace_.find(id); t && t->collider) {
    float size[2] = {t->collider->w, t->collider->h};
    if (ImGui::DragFloat2("Size", size, 0.5f, ColliderData::kMinSize,
                          ColliderData::kMaxSize, "%.1f",
                          ImGuiSliderFlags_AlwaysClamp)) {
      t->collider->w = size[0];
      t->collider->h = size[1];
      mark_dirty();
    }
    track_inspector_item("Edit Collider Size");
  }
  e = workspace_.find(id);
  if (!e || !e->collider) {
    ImGui::PopID();
    return;
  }
  const ColliderData& now = *e->collider;
  if (now.trigger) {
    ImGui::TextDisabled("Fires enter / exit when a rider crosses; never blocks.");
  } else if (now.dynamic) {
    ImGui::TextDisabled("Stopped by walls; shoved apart from other dynamics.");
  } else {
    ImGui::TextDisabled("Blocks riders and dynamics; never moves.");
  }
  if (theme::SecondaryButton("Fit to Entity", ImVec2(120, 0))) {
    ColliderData fit = now;
    fit.offset_x = 0.0f;
    fit.offset_y = 0.0f;
    fit.w = e->w;
    fit.h = e->h;
    set_collider(id, fit, "Fit Collider");
  }
  ImGui::SameLine();
  if (theme::DangerButton("Remove Collider", ImVec2(130, 0))) {
    set_collider(id, std::nullopt, "Remove Collider");
  }
  ImGui::PopID();
}

// --- Tile Palette: solid toggle -------------------------------------------------------

void Editor2DScreen::draw_tile_solid_controls(const Entity2D* target) {
  const std::string tileset = (target && target->tilemap)
                                  ? target->tilemap->tileset
                                  : std::string();
  bool solid = tile_solid(tileset, brush_tile_);
  ImGui::BeginDisabled(is_playing());
  if (ImGui::Checkbox("Solid##brush_solid", &solid)) {
    set_tile_solid(tileset, brush_tile_, solid);
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip(
        "Solid tiles stop riders. Right-click any swatch to toggle it.\n"
        "Kept per tileset in scene.json (tile_solidity); K shows the walls.");
  }
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
