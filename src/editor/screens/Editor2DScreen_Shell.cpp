#include "editor/screens/Editor2DScreen.h"

#include "editor/ui/Theme.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <optional>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

void Editor2DScreen::setup_default_dock_layout(unsigned int dockspace_id) {
  if (dock_layout_initialized_) {
    return;
  }
  // Only build once when the dockspace has no saved layout yet.
  if (ImGui::DockBuilderGetNode(dockspace_id) != nullptr) {
    dock_layout_initialized_ = true;
    return;
  }
  dock_layout_initialized_ = true;

  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

  ImGuiID dock_main = dockspace_id;
  ImGuiID dock_left = 0;
  ImGuiID dock_right = 0;
  ImGuiID dock_center = 0;

  ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.22f, &dock_left,
                              &dock_main);
  ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.30f, &dock_right,
                              &dock_center);

  ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
  ImGui::DockBuilderDockWindow("Viewport2D", dock_center);
  ImGui::DockBuilderDockWindow("Inspector", dock_right);
  ImGui::DockBuilderFinish(dockspace_id);
}

void Editor2DScreen::draw_menu_bar() {
  if (!ImGui::BeginMainMenuBar()) {
    return;
  }

  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
      std::string err;
      if (!save_scene(&err) && !err.empty()) {
        std::cout << "[Editor2D] save failed: " << err << '\n';
      }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Back to Projects")) {
      request_back_to_projects();
    }
    if (ImGui::MenuItem("Quit", "Alt+F4")) {
      request_quit();
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Edit")) {
    if (ImGui::MenuItem("Create Entity")) {
      workspace_.create_entity("Entity");
      mark_dirty_and_autosave();
    }
    const bool has_sel = workspace_.selected() != nullptr;
    if (ImGui::MenuItem("Delete Selected", "Del", false, has_sel)) {
      if (const Entity2D* e = workspace_.selected()) {
        if (workspace_.delete_entity(e->id)) {
          mark_dirty_and_autosave();
        }
      }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Reset Scene Placeholders")) {
      workspace_.reset_defaults();
      mark_dirty_and_autosave();
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("View")) {
    ImGui::MenuItem("Hierarchy", nullptr, &show_hierarchy_);
    ImGui::MenuItem("Viewport", nullptr, &show_viewport_);
    ImGui::MenuItem("Inspector", nullptr, &show_inspector_);
    ImGui::MenuItem("Toolbar", nullptr, &show_toolbar_);
    ImGui::MenuItem("Status Bar", nullptr, &show_status_bar_);
    ImGui::Separator();
    bool grid = workspace_.show_grid();
    if (ImGui::MenuItem("Show Grid", "G", grid)) {
      workspace_.set_show_grid(!grid);
      mark_dirty_and_autosave();
    }
    ImGui::MenuItem("Snap (placeholder)", nullptr, &snap_enabled_);
    if (ImGui::MenuItem("Reset Camera")) {
      workspace_.set_pan(0.0f, 0.0f);
      workspace_.set_zoom(1.0f);
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Help")) {
    ImGui::MenuItem("TombStone Admin Editor", nullptr, false, false);
    ImGui::Separator();
    ImGui::TextDisabled("Ctrl+S  Save scene.json");
    ImGui::TextDisabled("MMB / Alt+LMB  Pan viewport");
    ImGui::TextDisabled("Wheel  Zoom toward cursor");
    ImGui::TextDisabled("Click  Select entity");
    ImGui::EndMenu();
  }

  ImGui::TextDisabled("  |  %s (%s)%s", project_.name.c_str(),
                      to_string(project_.kind), dirty_ ? "  *" : "");
  ImGui::EndMainMenuBar();
}

void Editor2DScreen::draw_toolbar() {
  if (!show_toolbar_) {
    return;
  }
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  const float bar_h = 36.0f;
  ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y));
  ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, bar_h));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::HeaderBg());
  ImGui::Begin("##EditorToolbar", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav);
  bool grid = workspace_.show_grid();
  if (ImGui::Checkbox("Grid", &grid)) {
    workspace_.set_show_grid(grid);
    mark_dirty_and_autosave();
  }
  ImGui::SameLine();
  ImGui::Checkbox("Snap", &snap_enabled_);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Snap-to-grid placeholder (not applied yet).");
  }
  ImGui::SameLine();
  ImGui::Dummy(ImVec2(12, 0));
  ImGui::SameLine();
  if (theme::SecondaryButton("Reset Cam", ImVec2(90, 0))) {
    workspace_.set_pan(0.0f, 0.0f);
    workspace_.set_zoom(1.0f);
  }
  ImGui::SameLine();
  if (theme::PrimaryButton("Save", ImVec2(70, 0))) {
    std::string err;
    save_scene(&err);
  }
  ImGui::SameLine();
  ImGui::TextDisabled("  zoom %.0f%%", workspace_.zoom() * 100.0f);
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
}

void Editor2DScreen::draw_status_bar() {
  if (!show_status_bar_) {
    return;
  }
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  const float bar_h = 26.0f;
  ImGui::SetNextWindowPos(
      ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - bar_h));
  ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, bar_h));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::HeaderBg());
  ImGui::Begin("##EditorStatusBar", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav);

  const ImGuiIO& io = ImGui::GetIO();
  const float fps = io.Framerate;
  const float ms = (fps > 0.01f) ? (1000.0f / fps) : 0.0f;
  ImGui::Text("FPS %.0f (%.1f ms)", fps, ms);
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
  ImGui::Text("Zoom %.0f%%", workspace_.zoom() * 100.0f);
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
  if (const Entity2D* e = workspace_.selected()) {
    ImGui::Text("Selected: %s", e->name.c_str());
  } else {
    ImGui::TextDisabled("Selected: (none)");
  }
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
  ImGui::TextDisabled("%s", project_.path.c_str());
  if (dirty_) {
    ImGui::SameLine();
    ImGui::TextColored(theme::Warning(), "  unsaved*");
  }

  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
}

void Editor2DScreen::draw_ui() {
  draw_menu_bar();
  draw_toolbar();
  draw_status_bar();

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  float top_pad = show_toolbar_ ? 36.0f : 0.0f;
  float bottom_pad = show_status_bar_ ? 26.0f : 0.0f;

  ImGui::SetNextWindowPos(
      ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + top_pad));
  ImGui::SetNextWindowSize(
      ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - top_pad - bottom_pad));

  ImGuiWindowFlags host_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
      ImGuiWindowFlags_NoDocking;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("##EditorDockHost", nullptr, host_flags);
  ImGui::PopStyleVar();

  const ImGuiID dockspace_id = ImGui::GetID("Editor2DDockspace");
  setup_default_dock_layout(dockspace_id);
  ImGui::DockSpace(dockspace_id, ImVec2(0, 0),
                   ImGuiDockNodeFlags_PassthruCentralNode);
  ImGui::End();

  if (show_hierarchy_) {
    ImGui::Begin("Hierarchy", &show_hierarchy_);
    draw_hierarchy();
    ImGui::End();
  }
  if (show_viewport_) {
    ImGui::Begin("Viewport2D", &show_viewport_);
    draw_viewport();
  ImGui::End();
  }
  if (show_inspector_) {
    ImGui::Begin("Inspector", &show_inspector_);
    draw_inspector();
    ImGui::End();
  }

  // Hotkeys beyond Ctrl+S (already in on_update).
  if (ImGui::IsKeyPressed(ImGuiKey_Delete) && workspace_.selected() &&
      !ImGui::GetIO().WantTextInput) {
    if (const Entity2D* e = workspace_.selected()) {
      if (workspace_.delete_entity(e->id)) {
        mark_dirty_and_autosave();
      }
    }
  }
  if (ImGui::IsKeyPressed(ImGuiKey_G) && !ImGui::GetIO().WantTextInput) {
    workspace_.set_show_grid(!workspace_.show_grid());
    mark_dirty_and_autosave();
  }
}


}  // namespace editor
}  // namespace tombstone
}  // namespace ts
