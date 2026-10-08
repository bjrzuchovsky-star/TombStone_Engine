#include "editor/screens/Editor2DScreen.h"

#include "editor/assets/AssetLibrary.h"
#include "editor/ui/Theme.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

namespace {

ImU32 color_u32(const float c[4]) {
  return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));
}

ImTextureID tex_id(std::uint64_t handle) {
  return (ImTextureID)(std::uintptr_t)handle;
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

// Small amber "!" flag in the top-right corner: sprite image missing.
void draw_missing_marker(ImDrawList* draw, const ImVec2& p0, const ImVec2& p1) {
  const float s = 14.0f;
  const ImVec2 a(p1.x - s - 2.0f, p0.y + 2.0f);
  const ImVec2 tip(a.x + s * 0.5f, a.y);
  const ImVec2 bl(a.x, a.y + s);
  const ImVec2 br(a.x + s, a.y + s);
  draw->AddTriangleFilled(tip, br, bl, theme::U32(theme::Warning()));
  draw->AddTriangle(tip, br, bl, theme::U32(theme::Charcoal(), 0.9f), 1.0f);
  draw->AddText(ImVec2(tip.x - 2.0f, a.y + 1.0f),
                theme::U32(theme::Charcoal()), "!");
}

}  // namespace

void Editor2DScreen::draw_viewport() {
  viewport_focused_ =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
  handle_collision_hotkey();

  const bool tile_mode = tool_ != TileTool::Select;
  ImGui::PushStyleColor(ImGuiCol_Text, theme::TextMuted());
  if (tile_mode) {
    ImGui::Text(
        "%s: LMB on the selected TileMap | Alt+click pick tile | V select | "
        "MMB/RMB pan | wheel zoom",
        to_string(tool_));
  } else {
    ImGui::TextUnformatted(
        "LMB drag move | Ctrl+click multi | drag empty: box select | "
        "MMB/RMB/Alt pan | wheel zoom | drop assets here | K collision");
  }
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

  // Alt+click over the target TileMap picks a tile instead of panning.
  std::uint64_t target = paint_target();
  bool alt_pick = false;
  if (tile_mode && alt && target != 0 && hovered) {
    const ViewXform pre{workspace_.pan_x(), workspace_.pan_y(),
                        workspace_.zoom(), canvas_pos, canvas_size};
    const ImVec2 w = pre.to_world(io.MousePos.x, io.MousePos.y);
    alt_pick = workspace_.world_to_cell(target, w.x, w.y, nullptr, nullptr);
  }

  // --- Camera: MMB / RMB / Alt+LMB pan, wheel zoom -------------------------
  if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
                  ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
                  (alt && !alt_pick &&
                   ImGui::IsMouseClicked(ImGuiMouseButton_Left)))) {
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
  const ImVec2 mouse_world = view.to_world(io.MousePos.x, io.MousePos.y);

  // --- Supply Wagon drop: new sprite entity at the drop point --------------
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload(assets::kDragPayload)) {
      const char* data = static_cast<const char*>(payload->Data);
      std::size_t len = 0;
      while (len < static_cast<std::size_t>(payload->DataSize) &&
             data[len] != '\0') {
        ++len;
      }
      const std::string rel(data, len);
      tool_ = TileTool::Select;
      create_sprite_at(rel, mouse_world.x, mouse_world.y);
    }
    ImGui::EndDragDropTarget();
  }

  // Cell under the cursor for whichever TileMap we are painting.
  auto cell_for = [&](std::uint64_t id, int* c, int* r) {
    return workspace_.world_to_cell(id, mouse_world.x, mouse_world.y, c, r);
  };

  // --- Tile tools -----------------------------------------------------------
  if (tile_mode && hovered && !panning_ &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    cancel_rename();
    int c = 0;
    int r = 0;
    if (alt_pick) {
      cell_for(target, &c, &r);
      eyedrop(target, c, r);
    } else {
      // Clicking a different TileMap retargets the brush to it.
      const std::optional<std::uint64_t> tm_hit =
          workspace_.pick_tilemap(mouse_world.x, mouse_world.y);
      if (tm_hit && *tm_hit != target) {
        workspace_.select(*tm_hit);
        target = *tm_hit;
      }
      if (target == 0) {
        note("No TileMap in the crosshairs. Select one or Edit > Create "
             "TileMap.");
      } else {
        const bool inside = cell_for(target, &c, &r);
        switch (tool_) {
          case TileTool::Paint:
          case TileTool::Erase:
            if (begin_paint_stroke(target, tool_ == TileTool::Erase)) {
              stroke_to_cell(c, r);
              drag_mode_ = DragMode::Paint;
            }
            break;
          case TileTool::Fill:
            if (inside) {
              fill_at(target, c, r);
            }
            break;
          case TileTool::Rect:
            if (inside) {
              rect_c0_ = c;
              rect_r0_ = r;
              drag_mode_ = DragMode::RectFill;
            }
            break;
          case TileTool::Eyedropper:
            if (inside && eyedrop(target, c, r)) {
              set_tool(TileTool::Paint);
            }
            break;
          case TileTool::Select:
            break;
        }
      }
    }
  }

  if (drag_mode_ == DragMode::Paint || drag_mode_ == DragMode::RectFill) {
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      if (drag_mode_ == DragMode::Paint && stroke_active()) {
        int c = 0;
        int r = 0;
        cell_for(stroke_id_, &c, &r);
        stroke_to_cell(c, r);
      }
      if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        if (drag_mode_ == DragMode::Paint) {
          cancel_paint_stroke();
        } else {
          note("Rect called off");
        }
        drag_mode_ = DragMode::None;
      }
    } else {
      if (drag_mode_ == DragMode::Paint) {
        end_paint_stroke();
      } else if (target != 0) {
        int c = 0;
        int r = 0;
        cell_for(target, &c, &r);
        fill_rect_cells(target, rect_c0_, rect_r0_, c, r, shift);
      }
      drag_mode_ = DragMode::None;
    }
  }

  // --- Selection / drag-move / marquee ---------------------------------------
  if (!tile_mode && hovered && !alt && !panning_ &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    cancel_rename();
    const std::optional<std::uint64_t> hit =
        workspace_.pick(mouse_world.x, mouse_world.y);
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

  if ((drag_mode_ == DragMode::Move || drag_mode_ == DragMode::Marquee) &&
      ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
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
  } else if (drag_mode_ == DragMode::Move || drag_mode_ == DragMode::Marquee) {
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
        workspace_.select_in_rect(wa.x, wa.y, mouse_world.x, mouse_world.y,
                                  drag_additive_);
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
  if (!tile_mode && hovered && drag_mode_ == DragMode::None && !panning_) {
    hover_id = workspace_.pick(mouse_world.x, mouse_world.y);
  }
  const ImVec2 world_min = view.to_world(canvas_pos.x, canvas_pos.y);
  const ImVec2 world_max = view.to_world(canvas_end.x, canvas_end.y);
  const ImU32 primary_col = theme::U32(theme::Accent());
  const ImU32 secondary_col = theme::U32(theme::Sand());
  const ImU32 hover_col = theme::U32(theme::Copper(), 0.9f);
  const ImU32 edge_col = theme::U32(theme::Charcoal(), 0.8f);
  for (std::size_t idx : workspace_.sorted_draw_order()) {
    const Entity2D& e = workspace_.entities()[idx];
    const ImVec2 p0 = view.to_screen(e.x, e.y);
    const ImVec2 p1 = view.to_screen(e.x + e.w, e.y + e.h);
    if (e.tilemap) {
      const TileMapData& tm = *e.tilemap;
      // Empty ground: the entity tint, faint, so the map bounds read.
      ImVec4 bg(e.color[0], e.color[1], e.color[2], e.color[3] * 0.22f);
      draw->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(bg));
      const bool has_set = tileset_ready(e);
      const TextureInfo* set = has_set ? &texture(tm.tileset) : nullptr;
      const int count = palette_count(e);
      const float ts = static_cast<float>(tm.tile_size);
      const int c0 = std::max(0, static_cast<int>(std::floor((world_min.x - e.x) / ts)));
      const int r0 = std::max(0, static_cast<int>(std::floor((world_min.y - e.y) / ts)));
      const int c1 = std::min(tm.cols - 1, static_cast<int>(std::floor((world_max.x - e.x) / ts)));
      const int r1 = std::min(tm.rows - 1, static_cast<int>(std::floor((world_max.y - e.y) / ts)));
      for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
          const int t = tm.at(c, r);
          if (t <= 0) {
            continue;
          }
          const ImVec2 q0 = view.to_screen(e.x + c * ts, e.y + r * ts);
          const ImVec2 q1 = view.to_screen(e.x + (c + 1) * ts, e.y + (r + 1) * ts);
          draw_tile_quad(draw, set, tm.tile_size, count, t, q0, q1,
                         e.color[3]);
        }
      }
      // Cell lines on the map being painted.
      const float cell_px = ts * view.zoom;
      if (e.id == paint_target() && tile_mode && cell_px >= 6.0f) {
        const ImU32 line = theme::U32(theme::Sand(), 0.18f);
        for (int c = 1; c < tm.cols; ++c) {
          const float x = p0.x + c * cell_px;
          if (x >= canvas_pos.x && x <= canvas_end.x) {
            draw->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), line);
          }
        }
        for (int r = 1; r < tm.rows; ++r) {
          const float y = p0.y + r * cell_px;
          if (y >= canvas_pos.y && y <= canvas_end.y) {
            draw->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y), line);
          }
        }
      }
    } else if (e.sprite) {
      if (sprite_state(e) == SpriteState::Ready &&
          texture(e.sprite->path).handle != 0) {
        const SpriteData& sp = *e.sprite;
        const TextureInfo& t = texture(sp.path);
        ImVec2 uv0(0.0f, 0.0f);
        ImVec2 uv1(1.0f, 1.0f);
        if (sp.use_src_rect && t.width > 0 && t.height > 0) {
          const int sw = sp.src_w > 0 ? sp.src_w : t.width - sp.src_x;
          const int sh = sp.src_h > 0 ? sp.src_h : t.height - sp.src_y;
          uv0 = ImVec2(static_cast<float>(sp.src_x) / t.width,
                       static_cast<float>(sp.src_y) / t.height);
          uv1 = ImVec2(static_cast<float>(sp.src_x + sw) / t.width,
                       static_cast<float>(sp.src_y + sh) / t.height);
        }
        if (sp.flip_x) std::swap(uv0.x, uv1.x);
        if (sp.flip_y) std::swap(uv0.y, uv1.y);
        draw->AddImage(tex_id(t.handle), p0, p1, uv0, uv1, color_u32(e.color));
      } else {
        draw->AddRectFilled(p0, p1, color_u32(e.color), 1.0f);
        draw_missing_marker(draw, p0, p1);
      }
    } else {
      draw->AddRectFilled(p0, p1, color_u32(e.color), 1.0f);
    }
    const bool selected = workspace_.is_selected(e.id);
    const bool primary =
        workspace_.selected_id().has_value() && *workspace_.selected_id() == e.id;
    if (primary) {
      draw->AddRect(p0, p1, primary_col, 1.0f, 0, 2.5f);
      if (!(tile_mode && e.tilemap)) {
        draw_corner_handles(draw, p0, p1, primary_col);
      }
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

  // --- Collision overlay (K) ---------------------------------------------------
  draw_collision_overlay(draw, canvas_pos, canvas_size, view.pan_x, view.pan_y,
                         view.zoom);

  // --- Tile brush preview / hover cell ---------------------------------------
  hover_cell_valid_ = false;
  if (tile_mode && target != 0 && (hovered || drag_mode_ != DragMode::None)) {
    const Entity2D* tme = workspace_.find(target);
    int c = 0;
    int r = 0;
    const bool inside = cell_for(target, &c, &r);
    if (tme && tme->tilemap) {
      const TileMapData& tm = *tme->tilemap;
      const float ts = static_cast<float>(tm.tile_size);
      auto cell_rect = [&](int cc, int rr, ImVec2* a, ImVec2* b) {
        *a = view.to_screen(tme->x + cc * ts, tme->y + rr * ts);
        *b = view.to_screen(tme->x + (cc + 1) * ts, tme->y + (rr + 1) * ts);
      };
      if (inside) {
        hover_cell_valid_ = true;
        hover_col_ = c;
        hover_row_ = r;
        hover_tile_ = tm.at(c, r);
      }
      const bool picking = alt || tool_ == TileTool::Eyedropper;
      if (drag_mode_ == DragMode::RectFill) {
        const int cmin = std::clamp(std::min(rect_c0_, c), 0, tm.cols - 1);
        const int cmax = std::clamp(std::max(rect_c0_, c), 0, tm.cols - 1);
        const int rmin = std::clamp(std::min(rect_r0_, r), 0, tm.rows - 1);
        const int rmax = std::clamp(std::max(rect_r0_, r), 0, tm.rows - 1);
        ImVec2 a, b, a2, b2;
        cell_rect(cmin, rmin, &a, &b);
        cell_rect(cmax, rmax, &a2, &b2);
        if (!shift) {
          const bool has_set = tileset_ready(*tme);
          const TextureInfo* set = has_set ? &texture(tm.tileset) : nullptr;
          const int count = palette_count(*tme);
          for (int rr = rmin; rr <= rmax; ++rr) {
            for (int cc = cmin; cc <= cmax; ++cc) {
              ImVec2 q0, q1;
              cell_rect(cc, rr, &q0, &q1);
              draw_tile_quad(draw, set, tm.tile_size, count, brush_tile_, q0,
                             q1, 0.55f);
            }
          }
        } else {
          draw->AddRectFilled(a, b2, theme::U32(theme::Danger(), 0.25f));
        }
        draw->AddRect(a, b2, primary_col, 0.0f, 0, 2.0f);
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%d x %d", cmax - cmin + 1,
                      rmax - rmin + 1);
        draw->AddText(ImVec2(io.MousePos.x + 14.0f, io.MousePos.y + 10.0f),
                      primary_col, buf);
      } else if (picking) {
        if (inside) {
          ImVec2 a, b;
          cell_rect(c, r, &a, &b);
          draw->AddRect(a, b, theme::U32(theme::Sand()), 0.0f, 0, 2.0f);
          const ImVec2 sw0(io.MousePos.x + 14.0f, io.MousePos.y + 8.0f);
          const ImVec2 sw1(sw0.x + 20.0f, sw0.y + 20.0f);
          if (hover_tile_ > 0) {
            const bool has_set = tileset_ready(*tme);
            draw_tile_quad(draw, has_set ? &texture(tm.tileset) : nullptr,
                           tm.tile_size, palette_count(*tme), hover_tile_, sw0,
                           sw1);
          }
          draw->AddRect(sw0, sw1, theme::U32(theme::Sand()));
        }
      } else if (tool_ == TileTool::Fill) {
        if (inside) {
          ImVec2 a, b;
          cell_rect(c, r, &a, &b);
          const bool has_set = tileset_ready(*tme);
          draw_tile_quad(draw, has_set ? &texture(tm.tileset) : nullptr,
                         tm.tile_size, palette_count(*tme), brush_tile_, a, b,
                         0.6f);
          draw->AddRect(a, b, primary_col, 0.0f, 0, 2.0f);
          draw->AddText(ImVec2(b.x + 4.0f, a.y), primary_col, "fill");
        }
      } else {
        // Brush / erase footprint (clipped to the grid).
        const int n = brush_size_;
        const int bc0 = c - (n - 1) / 2;
        const int br0 = r - (n - 1) / 2;
        const bool erase = tool_ == TileTool::Erase;
        const bool has_set = tileset_ready(*tme);
        const TextureInfo* set = has_set ? &texture(tm.tileset) : nullptr;
        const int count = palette_count(*tme);
        bool any = false;
        for (int rr = br0; rr < br0 + n; ++rr) {
          for (int cc = bc0; cc < bc0 + n; ++cc) {
            if (!tm.in_bounds(cc, rr)) {
              continue;
            }
            any = true;
            ImVec2 a, b;
            cell_rect(cc, rr, &a, &b);
            if (erase) {
              draw->AddRectFilled(a, b, theme::U32(theme::Danger(), 0.25f));
              draw->AddLine(a, b, theme::U32(theme::Danger(), 0.8f), 1.5f);
              draw->AddLine(ImVec2(a.x, b.y), ImVec2(b.x, a.y),
                            theme::U32(theme::Danger(), 0.8f), 1.5f);
            } else {
              draw_tile_quad(draw, set, tm.tile_size, count, brush_tile_, a, b,
                             0.55f);
            }
          }
        }
        if (any) {
          ImVec2 a, b, a2, b2;
          cell_rect(bc0, br0, &a, &b);
          cell_rect(bc0 + n - 1, br0 + n - 1, &a2, &b2);
          draw->AddRect(a, b2, erase ? theme::U32(theme::Danger())
                                     : primary_col,
                        0.0f, 0, 2.0f);
        }
      }
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

  // Missing-sprite tooltip.
  if (hover_id) {
    if (const Entity2D* he = workspace_.find(*hover_id)) {
      if (he->sprite && sprite_state(*he) == SpriteState::Missing) {
        ImGui::SetTooltip("Sprite image missing: %s",
                          he->sprite->path.empty() ? "(no path)"
                                                   : he->sprite->path.c_str());
      }
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
