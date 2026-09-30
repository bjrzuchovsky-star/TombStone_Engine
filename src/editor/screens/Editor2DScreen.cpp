#include "editor/screens/Editor2DScreen.h"

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

Editor2DScreen::Editor2DScreen(ProjectInfo project)
    : project_(std::move(project)) {}

void Editor2DScreen::on_enter() {
  quit_requested_ = false;
  back_requested_ = false;
  renaming_ = false;
  panning_ = false;
  pending_click_select_ = false;
  workspace_.reset_defaults();
  std::cout << "[Editor2D] workspace for \"" << project_.name << "\" ("
            << to_string(project_.kind) << ") path=" << project_.path << '\n';
}

void Editor2DScreen::on_exit() {
  cancel_rename();
  std::cout << "[Editor2D] leaving workspace\n";
}

void Editor2DScreen::begin_rename(std::uint64_t id) {
  const Entity2D* e = workspace_.find(id);
  if (!e) {
    return;
  }
  renaming_ = true;
  rename_id_ = id;
  std::memset(rename_buf_, 0, sizeof(rename_buf_));
  std::strncpy(rename_buf_, e->name.c_str(), sizeof(rename_buf_) - 1);
}

void Editor2DScreen::commit_rename() {
  if (!renaming_) {
    return;
  }
  std::string name = rename_buf_;
  // trim
  while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
    name.pop_back();
  }
  if (!name.empty()) {
    workspace_.rename_entity(rename_id_, std::move(name));
  }
  renaming_ = false;
}

void Editor2DScreen::cancel_rename() {
  renaming_ = false;
}

void Editor2DScreen::draw_hierarchy() {
  ImGui::TextUnformatted("Scene Hierarchy");
  ImGui::TextDisabled("%zu entit%s", workspace_.entities().size(),
                      workspace_.entities().size() == 1 ? "y" : "ies");
  ImGui::Separator();

  if (ImGui::Button("Create", ImVec2(70, 0))) {
    cancel_rename();
    workspace_.create_entity("Entity");
  }
  ImGui::SameLine();
  const bool has_sel = workspace_.selected() != nullptr;
  if (!has_sel) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Rename", ImVec2(70, 0))) {
    if (const Entity2D* e = workspace_.selected()) {
      begin_rename(e->id);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Delete", ImVec2(70, 0))) {
    if (const Entity2D* e = workspace_.selected()) {
      const std::uint64_t id = e->id;
      cancel_rename();
      workspace_.delete_entity(id);
    }
  }
  if (!has_sel) {
    ImGui::EndDisabled();
  }

  ImGui::Separator();

  // Root scene node (non-selectable container).
  ImGui::SetNextItemOpen(true, ImGuiCond_Once);
  if (ImGui::TreeNodeEx("Scene##root",
                        ImGuiTreeNodeFlags_DefaultOpen |
                            ImGuiTreeNodeFlags_SpanAvailWidth)) {
    for (const Entity2D& e : workspace_.entities()) {
      const bool selected =
          workspace_.selected_id().has_value() &&
          *workspace_.selected_id() == e.id;

      if (renaming_ && rename_id_ == e.id) {
        ImGui::PushID(static_cast<int>(e.id));
        ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##rename", rename_buf_, sizeof(rename_buf_),
                             ImGuiInputTextFlags_EnterReturnsTrue |
                                 ImGuiInputTextFlags_AutoSelectAll)) {
          commit_rename();
        }
        if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0) &&
            !ImGui::IsItemHovered()) {
          commit_rename();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
          cancel_rename();
        }
        ImGui::PopID();
        continue;
      }

      ImGuiTreeNodeFlags flags =
          ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
          ImGuiTreeNodeFlags_SpanAvailWidth;
      if (selected) {
        flags |= ImGuiTreeNodeFlags_Selected;
      }
      ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(e.id)),
                        flags, "%s", e.name.c_str());
      if (ImGui::IsItemClicked()) {
        workspace_.select(e.id);
      }
      if (ImGui::IsItemHovered() &&
          ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        begin_rename(e.id);
      }
      if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Rename")) {
          begin_rename(e.id);
        }
        if (ImGui::MenuItem("Delete")) {
          workspace_.delete_entity(e.id);
        }
        ImGui::EndPopup();
      }
    }
    ImGui::TreePop();
  }
}

void Editor2DScreen::draw_viewport() {
  ImGui::Text("Viewport2D -- %s", project_.name.c_str());
  ImGui::SameLine();
  bool grid = workspace_.show_grid();
  if (ImGui::Checkbox("Grid", &grid)) {
    workspace_.set_show_grid(grid);
  }
  ImGui::SameLine();
  ImGui::TextDisabled("zoom %.2f  pan (%.0f, %.0f)  MMB/drag pan, wheel zoom",
                      workspace_.zoom(), workspace_.pan_x(),
                      workspace_.pan_y());
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
                      IM_COL32(28, 30, 36, 255));
  draw->AddRect(canvas_pos,
                ImVec2(canvas_pos.x + canvas_size.x,
                       canvas_pos.y + canvas_size.y),
                IM_COL32(60, 65, 75, 255));

  ImGui::InvisibleButton("##ViewportCanvas", canvas_size,
                         ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonMiddle);
  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();

  const float zoom = workspace_.zoom();
  const float pan_x = workspace_.pan_x();
  const float pan_y = workspace_.pan_y();

  // Pan: middle mouse, or left-drag on empty space (after click fails select).
  if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
    panning_ = true;
  }
  if (panning_ && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
    const ImVec2 delta = ImGui::GetIO().MouseDelta;
    workspace_.add_pan(-delta.x / zoom, -delta.y / zoom);
  } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
    panning_ = false;
  }

  // Also allow left-drag pan when holding Alt, or space+left (common editor UX).
  const bool alt_pan =
      hovered && ImGui::IsKeyDown(ImGuiKey_LeftAlt) &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left);
  if (alt_pan) {
    const ImVec2 delta = ImGui::GetIO().MouseDelta;
    workspace_.add_pan(-delta.x / zoom, -delta.y / zoom);
  }

  // Wheel zoom toward cursor.
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

  // Click select (top-most by layer).
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

  // Optional left-drag pan when dragging on empty (no selection change mid-drag).
  if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
      !ImGui::IsKeyDown(ImGuiKey_LeftAlt) &&
      !workspace_.selected_id().has_value()) {
    const ImVec2 delta = ImGui::GetIO().MouseDelta;
    workspace_.add_pan(-delta.x / zoom, -delta.y / zoom);
  }

  // Grid.
  if (workspace_.show_grid()) {
    const float gs = workspace_.grid_size() * workspace_.zoom();
    if (gs >= 4.0f) {
      const ImVec2 origin =
          world_to_screen(0.0f, 0.0f, workspace_.pan_x(),
                          workspace_.pan_y(), workspace_.zoom(), canvas_pos,
                          canvas_size);
      const float start_x =
          canvas_pos.x +
          std::fmod(origin.x - canvas_pos.x, gs);
      const float start_y =
          canvas_pos.y +
          std::fmod(origin.y - canvas_pos.y, gs);
      const ImU32 grid_col = IM_COL32(55, 58, 68, 180);
      for (float x = start_x; x < canvas_pos.x + canvas_size.x; x += gs) {
        draw->AddLine(ImVec2(x, canvas_pos.y),
                      ImVec2(x, canvas_pos.y + canvas_size.y), grid_col);
      }
      for (float y = start_y; y < canvas_pos.y + canvas_size.y; y += gs) {
        draw->AddLine(ImVec2(canvas_pos.x, y),
                      ImVec2(canvas_pos.x + canvas_size.x, y), grid_col);
      }
      // Axes.
      draw->AddLine(ImVec2(origin.x, canvas_pos.y),
                    ImVec2(origin.x, canvas_pos.y + canvas_size.y),
                    IM_COL32(90, 60, 60, 220));
      draw->AddLine(ImVec2(canvas_pos.x, origin.y),
                    ImVec2(canvas_pos.x + canvas_size.x, origin.y),
                    IM_COL32(60, 90, 60, 220));
    }
  }

  // Entities as filled rects (layer order).
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
    draw->AddRectFilled(p0, p1, color_u32(e.color));
    const bool selected =
        workspace_.selected_id().has_value() &&
        *workspace_.selected_id() == e.id;
    draw->AddRect(p0, p1,
                  selected ? IM_COL32(255, 220, 80, 255)
                           : IM_COL32(20, 20, 25, 200),
                  0.0f, 0, selected ? 2.5f : 1.0f);
    if (workspace_.zoom() >= 0.45f) {
      draw->AddText(ImVec2(p0.x + 4.0f, p0.y + 2.0f),
                    IM_COL32(255, 255, 255, 220), e.name.c_str());
    }
  }
  draw->PopClipRect();
}

void Editor2DScreen::draw_inspector() {
  ImGui::TextUnformatted("Inspector");
  ImGui::Separator();

  Entity2D* e = workspace_.selected();
  if (!e) {
    ImGui::TextDisabled("Selection: (none)");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Select an entity in Hierarchy or click it in the viewport.");
  } else {
    ImGui::Text("Selection: %s  (id %llu)", e->name.c_str(),
                static_cast<unsigned long long>(e->id));
    ImGui::Spacing();

    char name_buf[128];
    std::memset(name_buf, 0, sizeof(name_buf));
    std::strncpy(name_buf, e->name.c_str(), sizeof(name_buf) - 1);
    if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
      if (name_buf[0] != '\0') {
        e->name = name_buf;
      }
    }

    ImGui::SeparatorText("Transform");
    ImGui::DragFloat("X", &e->x, 0.5f);
    ImGui::DragFloat("Y", &e->y, 0.5f);
    ImGui::DragFloat("W", &e->w, 0.5f, 1.0f, 4096.0f);
    ImGui::DragFloat("H", &e->h, 0.5f, 1.0f, 4096.0f);

    ImGui::SeparatorText("Appearance");
    ImGui::ColorEdit4("Color / Tint", e->color,
                      ImGuiColorEditFlags_AlphaBar |
                          ImGuiColorEditFlags_Float);
    ImGui::InputInt("Layer / Z", &e->layer);
  }

  ImGui::Spacing();
  ImGui::Separator();
  if (ImGui::Button("Back to Projects", ImVec2(-1, 0))) {
    request_back_to_projects();
  }
}

void Editor2DScreen::draw_ui() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Back to Projects")) {
        request_back_to_projects();
      }
      if (ImGui::MenuItem("Quit")) {
        request_quit();
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Create Entity")) {
        workspace_.create_entity("Entity");
      }
      const bool has_sel = workspace_.selected() != nullptr;
      if (ImGui::MenuItem("Delete Selected", nullptr, false, has_sel)) {
        if (const Entity2D* e = workspace_.selected()) {
          workspace_.delete_entity(e->id);
        }
      }
      if (ImGui::MenuItem("Reset Scene Placeholders")) {
        workspace_.reset_defaults();
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      bool grid = workspace_.show_grid();
      if (ImGui::MenuItem("Show Grid", nullptr, grid)) {
        workspace_.set_show_grid(!grid);
      }
      if (ImGui::MenuItem("Reset Camera")) {
        workspace_.set_pan(0.0f, 0.0f);
        workspace_.set_zoom(1.0f);
      }
      ImGui::EndMenu();
    }
    ImGui::TextDisabled("  |  %s (%s)", project_.name.c_str(),
                        to_string(project_.kind));
    ImGui::EndMainMenuBar();
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImVec2 work_pos = viewport->WorkPos;
  const ImVec2 work_size = viewport->WorkSize;

  const float left_w = work_size.x * 0.22f;
  const float right_w = work_size.x * 0.28f;
  const float center_w = work_size.x - left_w - right_w;

  ImGui::SetNextWindowPos(work_pos);
  ImGui::SetNextWindowSize(ImVec2(left_w, work_size.y));
  ImGui::Begin("Hierarchy", nullptr,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove);
  draw_hierarchy();
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(work_pos.x + left_w, work_pos.y));
  ImGui::SetNextWindowSize(ImVec2(center_w, work_size.y));
  ImGui::Begin("Viewport2D", nullptr,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove);
  draw_viewport();
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(work_pos.x + left_w + center_w, work_pos.y));
  ImGui::SetNextWindowSize(ImVec2(right_w, work_size.y));
  ImGui::Begin("Inspector", nullptr,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove);
  draw_inspector();
  ImGui::End();
}

AppState Editor2DScreen::on_update(float /*delta_seconds*/) {
  if (ImGui::GetCurrentContext() != nullptr) {
    draw_ui();
  }

  if (quit_requested_) {
    return AppState::Quit;
  }
  if (back_requested_) {
    return AppState::ProjectManager;
  }
  return AppState::Editor2D;
}

void Editor2DScreen::request_quit() {
  quit_requested_ = true;
}

void Editor2DScreen::request_back_to_projects() {
  back_requested_ = true;
  std::cout << "[Editor2D] back to ProjectManager\n";
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
