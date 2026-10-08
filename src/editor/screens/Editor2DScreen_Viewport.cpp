#include "editor/screens/Editor2DScreen.h"

#include "editor/ui/Theme.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

namespace {

ImU32 color_u32(const float c[4]) {
  return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));
}

struct ViewXform {
  float pan_x;
  float pan_y;
  float zoom;
  ImVec2 pos;
  ImVec2 size;

  ImVec2 to_screen(float wx, float wy) const {
    return ImVec2(pos.x + size.x * 0.5f + (wx - pan_x) * zoom,
                  pos.y + size.y * 0.5f + (wy - pan_y) * zoom);
  }
  ImVec2 to_world(float sx, float sy) const {
    return ImVec2((sx - pos.x - size.x * 0.5f) / zoom + pan_x,
                  (sy - pos.y - size.y * 0.5f) / zoom + pan_y);
  }
};

void draw_corner_handles(ImDrawList* draw, const ImVec2& p0, const ImVec2& p1,
                         ImU32 col) {
  const float h = 3.0f;
  const ImVec2 corners[4] = {p0, ImVec2(p1.x, p0.y), p1, ImVec2(p0.x, p1.y)};
  for (const ImVec2& c : corners) {
    draw->AddRectFilled(ImVec2(c.x - h, c.y - h), ImVec2(c.x + h, c.y + h),
                        col);
  }
}

}  // namespace

void Editor2DScreen::draw_viewport() {
  viewport_focused_ =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

  ImGui::PushStyleColor(ImGuiCol_Text, theme::TextMuted());
  ImGui::TextUnformatted(
      "LMB drag move | Ctrl+click multi | drag empty: box select | "
      "MMB/RMB/Alt pan | wheel zoom");
  ImGui::PopStyleColor();

  const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
  ImVec2 canvas_size = ImGui::GetContentRegionAvail();
  canvas_size.x = std::max(canvas_size.x, 32.0f);
  canvas_size.y = std::max(canvas_size.y, 32.0f);
  const ImVec2 canvas_end(canvas_pos.x + canvas_size.x,
                          canvas_pos.y + canvas_size.y);

  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(canvas_pos, canvas_end, theme::U32(theme::CanvasBg()),
                      theme::metrics::kRadiusSmall);

  ImGui::InvisibleButton("##ViewportCanvas", canvas_size,
                         ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonMiddle |
                             ImGuiButtonFlags_MouseButtonRight);
  const bool hovered = ImGui::IsItemHovered();
  ImGuiIO& io = ImGui::GetIO();
  const bool alt = io.KeyAlt;
  const bool ctrl = io.KeyCtrl || io.KeySuper;
  const bool shift = io.KeyShift;

  // --- Camera: MMB / RMB / Alt+LMB pan, wheel zoom -------------------------
  if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
                  ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
                  (alt && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))) {
    panning_ = true;
  }
  const bool pan_button_down = ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                               ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
                               (alt && ImGui::IsMouseDown(ImGuiMouseButton_Left));
  if (panning_ && pan_button_down) {
    const ImVec2 delta = io.MouseDelta;
    workspace_.add_pan(-delta.x / workspace_.zoom(),
                       -delta.y / workspace_.zoom());
  } else {
    panning_ = false;
  }

  if (hovered && std::abs(io.MouseWheel) > 0.0f) {
    const float factor = (io.MouseWheel > 0.0f) ? 1.1f : (1.0f / 1.1f);
    workspace_.adjust_zoom(factor, io.MousePos.x - canvas_pos.x,
                           io.MousePos.y - canvas_pos.y, canvas_size.x,
                           canvas_size.y);
  }

  const ViewXform view{workspace_.pan_x(), workspace_.pan_y(),
                       workspace_.zoom(), canvas_pos, canvas_size};

  // --- Selection / drag-move / marquee ---------------------------------------
  if (hovered && !alt && !panning_ &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    cancel_rename();
    const ImVec2 w = view.to_world(io.MousePos.x, io.MousePos.y);
    const std::optional<std::uint64_t> hit = workspace_.pick(w.x, w.y);
    drag_hit_id_ = hit.value_or(0);
    drag_additive_ = ctrl || shift;
    drag_moved_ = false;
    drag_mode_ = DragMode::None;
    if (hit) {
      if (ctrl) {
        workspace_.toggle_selection(*hit);
        if (workspace_.is_selected(*hit)) {
          drag_mode_ = DragMode::Move;
        }
      } else if (shift) {
        workspace_.add_to_selection(*hit);
        drag_mode_ = DragMode::Move;
      } else {
        if (!workspace_.is_selected(*hit)) {
          workspace_.select(*hit);
        } else {
          workspace_.add_to_selection(*hit);  // make primary, keep group
        }
        drag_mode_ = DragMode::Move;
      }
      if (drag_mode_ == DragMode::Move) {
        begin_drag_move();  // whole drag = one undo step
      }
    } else {
      drag_mode_ = DragMode::Marquee;
    }
  }

  if (drag_mode_ != DragMode::None && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f)) {
      drag_moved_ = true;
    }
    if (drag_mode_ == DragMode::Move && drag_moved_) {
      const ImVec2 d = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
      workspace_.update_move(d.x / view.zoom, d.y / view.zoom);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      if (drag_mode_ == DragMode::Move) {
        cancel_drag_move();
        note("Move cancelled");
      }
      drag_mode_ = DragMode::None;
    }
  } else if (drag_mode_ != DragMode::None) {
    // Released (anywhere, even outside the canvas).
    if (drag_mode_ == DragMode::Move) {
      // end_drag_move() commits one "Move N" undo step + autosaves.
      const bool moved = end_drag_move();
      if (!moved && !drag_additive_ && !drag_moved_ && drag_hit_id_ != 0 &&
          workspace_.selection_count() > 1) {
        workspace_.select(drag_hit_id_);  // plain click on a group member
      }
    } else if (drag_mode_ == DragMode::Marquee) {
      if (drag_moved_) {
        const ImVec2 a = io.MouseClickedPos[ImGuiMouseButton_Left];
        const ImVec2 wa = view.to_world(a.x, a.y);
        const ImVec2 wb = view.to_world(io.MousePos.x, io.MousePos.y);
        workspace_.select_in_rect(wa.x, wa.y, wb.x, wb.y, drag_additive_);
      } else if (!drag_additive_) {
        workspace_.clear_selection();
      }
    }
    drag_mode_ = DragMode::None;
    drag_moved_ = false;
  }

  draw->PushClipRect(canvas_pos, canvas_end, true);

  // --- Grid -----------------------------------------------------------------
  if (workspace_.show_grid()) {
    const float gs = workspace_.grid_size() * view.zoom;
    const ImVec2 origin = view.to_screen(0.0f, 0.0f);
    if (gs >= 4.0f) {
      const ImU32 minor = theme::U32(theme::GridLine());
      const ImU32 major = theme::U32(theme::GridLine(), 2.2f);
      const float first_x = origin.x + std::floor((canvas_pos.x - origin.x) / gs) * gs;
      const float first_y = origin.y + std::floor((canvas_pos.y - origin.y) / gs) * gs;
      for (float x = first_x; x < canvas_end.x; x += gs) {
        const long idx = std::lround((x - origin.x) / gs);
        draw->AddLine(ImVec2(x, canvas_pos.y), ImVec2(x, canvas_end.y),
                      (idx % 4 == 0) ? major : minor);
      }
      for (float y = first_y; y < canvas_end.y; y += gs) {
        const long idx = std::lround((y - origin.y) / gs);
        draw->AddLine(ImVec2(canvas_pos.x, y), ImVec2(canvas_end.x, y),
                      (idx % 4 == 0) ? major : minor);
      }
    }
    draw->AddLine(ImVec2(origin.x, canvas_pos.y), ImVec2(origin.x, canvas_end.y),
                  theme::U32(theme::Copper(), 0.75f), 1.5f);
    draw->AddLine(ImVec2(canvas_pos.x, origin.y), ImVec2(canvas_end.x, origin.y),
                  theme::U32(theme::Success(), 0.6f), 1.5f);
  }

  // --- Entities ---------------------------------------------------------------
  std::optional<std::uint64_t> hover_id;
  if (hovered && drag_mode_ == DragMode::None && !panning_) {
    const ImVec2 w = view.to_world(io.MousePos.x, io.MousePos.y);
    hover_id = workspace_.pick(w.x, w.y);
  }
  const ImU32 primary_col = theme::U32(theme::Accent());
  const ImU32 secondary_col = theme::U32(theme::Sand());
  const ImU32 hover_col = theme::U32(theme::Copper(), 0.9f);
  const ImU32 edge_col = theme::U32(theme::Charcoal(), 0.8f);
  for (std::size_t idx : workspace_.sorted_draw_order()) {
    const Entity2D& e = workspace_.entities()[idx];
    const ImVec2 p0 = view.to_screen(e.x, e.y);
    const ImVec2 p1 = view.to_screen(e.x + e.w, e.y + e.h);
    draw->AddRectFilled(p0, p1, color_u32(e.color), 1.0f);
    const bool selected = workspace_.is_selected(e.id);
    const bool primary =
        workspace_.selected_id().has_value() && *workspace_.selected_id() == e.id;
    if (primary) {
      draw->AddRect(p0, p1, primary_col, 1.0f, 0, 2.5f);
      draw_corner_handles(draw, p0, p1, primary_col);
    } else if (selected) {
      draw->AddRect(p0, p1, secondary_col, 1.0f, 0, 2.0f);
    } else if (hover_id && *hover_id == e.id) {
      draw->AddRect(p0, p1, hover_col, 1.0f, 0, 1.5f);
    } else {
      draw->AddRect(p0, p1, edge_col, 1.0f, 0, 1.0f);
    }
    if (view.zoom >= 0.45f) {
      draw->AddText(ImVec2(p0.x + 4.0f, p0.y + 2.0f),
                    IM_COL32(255, 248, 236, 225), e.name.c_str());
    }
  }

  // --- Marquee ----------------------------------------------------------------
  if (drag_mode_ == DragMode::Marquee && drag_moved_) {
    const ImVec2 a = io.MouseClickedPos[ImGuiMouseButton_Left];
    const ImVec2 b = io.MousePos;
    const ImVec2 mn(std::min(a.x, b.x), std::min(a.y, b.y));
    const ImVec2 mx(std::max(a.x, b.x), std::max(a.y, b.y));
    draw->AddRectFilled(mn, mx, theme::U32(theme::Accent(), 0.12f));
    draw->AddRect(mn, mx, theme::U32(theme::Accent(), 0.9f), 0.0f, 0, 1.0f);
  }

  // --- Overlay: drag readout ----------------------------------------------------
  if (drag_mode_ == DragMode::Move && drag_moved_) {
    if (const Entity2D* p = workspace_.selected()) {
      char buf[96];
      std::snprintf(buf, sizeof(buf), "%.0f, %.0f%s", p->x, p->y,
                    workspace_.snap_enabled() ? "  [snap]" : "");
      const ImVec2 at(io.MousePos.x + 14.0f, io.MousePos.y + 10.0f);
      const ImVec2 ts = ImGui::CalcTextSize(buf);
      draw->AddRectFilled(ImVec2(at.x - 4.0f, at.y - 2.0f),
                          ImVec2(at.x + ts.x + 4.0f, at.y + ts.y + 2.0f),
                          theme::U32(theme::Charcoal(), 0.85f), 2.0f);
      draw->AddText(at, primary_col, buf);
    }
  }
  draw->PopClipRect();

  draw->AddRect(canvas_pos, canvas_end,
                theme::U32(viewport_focused_ ? theme::AccentMuted()
                                             : theme::Border()),
                theme::metrics::kRadiusSmall);
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
