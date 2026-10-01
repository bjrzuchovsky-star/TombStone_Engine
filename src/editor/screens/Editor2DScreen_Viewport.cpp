#include "editor/screens/Editor2DScreen.h"

#include "editor/ui/Theme.h"
#include "editor/workspace/SceneIO.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

namespace {

ImU32 color_u32(const float c[4]) {
  return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));
}

ImVec2 world_to_screen(float wx, float wy, float pan_x, float pan_y,
                       float zoom, const ImVec2& canvas_pos,
                       const ImVec2& canvas_size) {
  const float sx = canvas_pos.x + canvas_size.x * 0.5f + (wx - pan_x) * zoom;
  const float sy = canvas_pos.y + canvas_size.y * 0.5f + (wy - pan_y) * zoom;
  return ImVec2(sx, sy);
}

ImVec2 screen_to_world(float sx, float sy, float pan_x, float pan_y,
                       float zoom, const ImVec2& canvas_pos,
                       const ImVec2& canvas_size) {
  const float wx = (sx - canvas_pos.x - canvas_size.x * 0.5f) / zoom + pan_x;
  const float wy = (sy - canvas_pos.y - canvas_size.y * 0.5f) / zoom + pan_y;
  return ImVec2(wx, wy);
}

bool point_in_entity(float wx, float wy, const Entity2D& e) {
  return wx >= e.x && wy >= e.y && wx <= e.x + e.w && wy <= e.y + e.h;
}

}  // namespace

void Editor2DScreen::draw_viewport() {
  theme::SectionHeader("Viewport2D");
  ImGui::TextDisabled("%s  |  MMB/Alt-drag pan, wheel zoom, click select",
                      project_.name.c_str());
  ImGui::Separator();

  const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
  ImVec2 canvas_size = ImGui::GetContentRegionAvail();
  if (canvas_size.x < 32.0f) {
    canvas_size.x = 32.0f;
  }
  if (canvas_size.y < 32.0f) {
    canvas_size.y = 32.0f;
  }

  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(canvas_pos,
                      ImVec2(canvas_pos.x + canvas_size.x,
                             canvas_pos.y + canvas_size.y),
                      IM_COL32(22, 24, 30, 255), 4.0f);
  draw->AddRect(canvas_pos,
                ImVec2(canvas_pos.x + canvas_size.x,
                       canvas_pos.y + canvas_size.y),
                ImGui::ColorConvertFloat4ToU32(theme::AccentMuted()), 4.0f);

  ImGui::InvisibleButton("##ViewportCanvas", canvas_size,
                         ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonMiddle);
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();

  const float zoom = workspace_.zoom();
  const float pan_x = workspace_.pan_x();
  const float pan_y = workspace_.pan_y();

  if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
    panning_ = true;
  }
  if (panning_ && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
    const ImVec2 delta = ImGui::GetIO().MouseDelta;
    workspace_.add_pan(-delta.x / zoom, -delta.y / zoom);
  } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
    panning_ = false;
  }

  const bool alt_pan =
      hovered && ImGui::IsKeyDown(ImGuiKey_LeftAlt) &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left);
  if (alt_pan) {
    const ImVec2 delta = ImGui::GetIO().MouseDelta;
    workspace_.add_pan(-delta.x / zoom, -delta.y / zoom);
  }

  if (hovered) {
    const float wheel = ImGui::GetIO().MouseWheel;
    if (std::abs(wheel) > 0.0f) {
      const ImVec2 mouse = ImGui::GetIO().MousePos;
      const float factor = (wheel > 0.0f) ? 1.1f : (1.0f / 1.1f);
      workspace_.adjust_zoom(factor, mouse.x - canvas_pos.x,
                             mouse.y - canvas_pos.y, canvas_size.x,
                             canvas_size.y);
    }
  }

  if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
      !ImGui::IsKeyDown(ImGuiKey_LeftAlt)) {
    pending_click_select_ = true;
  }
  if (pending_click_select_ &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    pending_click_select_ = false;
    if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f)) {
      const ImVec2 mouse = ImGui::GetIO().MousePos;
      const ImVec2 world =
          screen_to_world(mouse.x, mouse.y, workspace_.pan_x(),
                          workspace_.pan_y(), workspace_.zoom(), canvas_pos,
                          canvas_size);
      std::optional<std::uint64_t> hit;
      int best_layer = -1;
      for (const Entity2D& e : workspace_.entities()) {
        if (point_in_entity(world.x, world.y, e)) {
          if (!hit || e.layer >= best_layer) {
            hit = e.id;
            best_layer = e.layer;
          }
        }
      }
      workspace_.select(hit);
    }
  }
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    pending_click_select_ = false;
  }

  if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
      !ImGui::IsKeyDown(ImGuiKey_LeftAlt) &&
      !workspace_.selected_id().has_value()) {
    const ImVec2 delta = ImGui::GetIO().MouseDelta;
    workspace_.add_pan(-delta.x / zoom, -delta.y / zoom);
  }

  if (workspace_.show_grid()) {
    const float gs = workspace_.grid_size() * workspace_.zoom();
    if (gs >= 4.0f) {
      const ImVec2 origin =
          world_to_screen(0.0f, 0.0f, workspace_.pan_x(),
                          workspace_.pan_y(), workspace_.zoom(), canvas_pos,
                          canvas_size);
      const float start_x =
          canvas_pos.x + std::fmod(origin.x - canvas_pos.x, gs);
      const float start_y =
          canvas_pos.y + std::fmod(origin.y - canvas_pos.y, gs);
      const ImU32 grid_col = IM_COL32(55, 58, 68, 180);
      for (float x = start_x; x < canvas_pos.x + canvas_size.x; x += gs) {
        draw->AddLine(ImVec2(x, canvas_pos.y),
                      ImVec2(x, canvas_pos.y + canvas_size.y), grid_col);
      }
      for (float y = start_y; y < canvas_pos.y + canvas_size.y; y += gs) {
        draw->AddLine(ImVec2(canvas_pos.x, y),
                      ImVec2(canvas_pos.x + canvas_size.x, y), grid_col);
      }
      draw->AddLine(ImVec2(origin.x, canvas_pos.y),
                    ImVec2(origin.x, canvas_pos.y + canvas_size.y),
                    IM_COL32(120, 70, 70, 220));
      draw->AddLine(ImVec2(canvas_pos.x, origin.y),
                    ImVec2(canvas_pos.x + canvas_size.x, origin.y),
                    IM_COL32(70, 120, 70, 220));
    }
  }

  draw->PushClipRect(canvas_pos,
                     ImVec2(canvas_pos.x + canvas_size.x,
                            canvas_pos.y + canvas_size.y),
                     true);
  for (std::size_t idx : workspace_.sorted_draw_order()) {
    const Entity2D& e = workspace_.entities()[idx];
    const ImVec2 p0 =
        world_to_screen(e.x, e.y, workspace_.pan_x(), workspace_.pan_y(),
                        workspace_.zoom(), canvas_pos, canvas_size);
    const ImVec2 p1 =
        world_to_screen(e.x + e.w, e.y + e.h, workspace_.pan_x(),
                        workspace_.pan_y(), workspace_.zoom(), canvas_pos,
                        canvas_size);
    draw->AddRectFilled(p0, p1, color_u32(e.color), 2.0f);
    const bool selected =
        workspace_.selected_id().has_value() &&
        *workspace_.selected_id() == e.id;
    draw->AddRect(p0, p1,
                  selected ? IM_COL32(255, 220, 80, 255)
                           : IM_COL32(20, 20, 25, 200),
                  2.0f, 0, selected ? 2.5f : 1.0f);
    if (workspace_.zoom() >= 0.45f) {
      draw->AddText(ImVec2(p0.x + 4.0f, p0.y + 2.0f),
                    IM_COL32(255, 255, 255, 220), e.name.c_str());
    }
  }
  draw->PopClipRect();

  (void)pan_x;
  (void)pan_y;
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
