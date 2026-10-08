#include "editor/screens/Editor2DScreen.h"

#include "editor/ui/Brand.h"
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

namespace {

constexpr float kGridPresets[] = {8.0f, 16.0f, 32.0f, 64.0f, 128.0f};

}  // namespace

void Editor2DScreen::draw_help_menu_contents() {
  ImGui::PushStyleColor(ImGuiCol_Text, theme::Accent());
  ImGui::TextUnformatted("TombStone Admin -- 2D field kit");
  ImGui::PopStyleColor();
  ImGui::Separator();
  theme::KeyHint("LMB drag", "Move selection");
  theme::KeyHint("Ctrl+click", "Add / remove from selection");
  theme::KeyHint("Shift+click", "Add (Hierarchy: range)");
  theme::KeyHint("Drag empty", "Box select (Ctrl/Shift adds)");
  theme::KeyHint("Arrows", "Nudge 1px / 1 cell (snap)");
  theme::KeyHint("Shift+Arrows", "Nudge 10px / 4 cells (snap)");
  theme::KeyHint("Ctrl+D", "Duplicate selection");
  theme::KeyHint("Del", "Delete selection");
  theme::KeyHint("Ctrl+A / Esc", "Select all / clear (Esc cancels drag)");
  theme::KeyHint("F2", "Rename primary");
  theme::KeyHint("G / Shift+G", "Toggle grid / snap");
  theme::KeyHint("[ / ]", "Halve / double grid size");
  theme::KeyHint("MMB/RMB/Alt", "Pan viewport");
  theme::KeyHint("Wheel", "Zoom toward cursor");
  theme::KeyHint("Ctrl+S", "Save scene.json");
}

void Editor2DScreen::draw_menu_bar() {
  if (!ImGui::BeginMainMenuBar()) {
    return;
  }

  // Brand mark in the menu strip.
  {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = ImGui::GetFrameHeight();
    theme::TombstoneMark(dl, ImVec2(p.x + 7.0f, p.y + 3.0f), h - 6.0f,
                         theme::U32(theme::Sand()),
                         theme::U32(theme::Charcoal(), 0.85f));
    ImGui::Dummy(ImVec2(16.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::Accent());
    ImGui::TextUnformatted(brand::kProduct);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(6.0f, 0.0f));
  }

  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
      std::string err;
      if (!save_scene(&err) && !err.empty()) {
        std::cout << "[Editor2D] save failed: " << err << '\n';
      } else {
        note("Scene saved");
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
    const bool has_sel = workspace_.selection_count() > 0;
    if (ImGui::MenuItem("Create Entity")) {
      cancel_rename();
      workspace_.create_entity("Entity");
      mark_dirty_and_autosave();
    }
    if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, has_sel)) {
      duplicate_selected();
    }
    if (ImGui::MenuItem("Delete Selected", "Del", false, has_sel)) {
      delete_selected();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Select All", "Ctrl+A")) {
      workspace_.select_all();
    }
    if (ImGui::MenuItem("Clear Selection", "Esc", false, has_sel)) {
      workspace_.clear_selection();
    }
    if (ImGui::MenuItem("Snap Selection to Grid", nullptr, false, has_sel)) {
      if (workspace_.snap_selection_to_grid()) {
        mark_dirty_and_autosave();
        note("Snapped to grid");
      }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Reset Scene Placeholders")) {
      cancel_rename();
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
    const bool grid = workspace_.show_grid();
    if (ImGui::MenuItem("Show Grid", "G", grid)) {
      workspace_.set_show_grid(!grid);
      mark_dirty_and_autosave();
    }
    const bool snap = workspace_.snap_enabled();
    if (ImGui::MenuItem("Snap to Grid", "Shift+G", snap)) {
      workspace_.set_snap_enabled(!snap);
      mark_dirty_and_autosave();
    }
    if (ImGui::BeginMenu("Grid Size")) {
      for (float g : kGridPresets) {
        char label[16];
        std::snprintf(label, sizeof(label), "%.0f px", g);
        if (ImGui::MenuItem(label, nullptr, workspace_.grid_size() == g)) {
          workspace_.set_grid_size(g);
          mark_dirty_and_autosave();
        }
      }
      ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Reset Camera")) {
      workspace_.set_pan(0.0f, 0.0f);
      workspace_.set_zoom(1.0f);
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Help")) {
    draw_help_menu_contents();
    ImGui::EndMenu();
  }

  ImGui::TextDisabled("|");
  ImGui::TextUnformatted(project_.name.c_str());
  ImGui::TextDisabled("(%s)", to_string(project_.kind));
  if (dirty_) {
    ImGui::TextColored(theme::Copper(), "*");
  }
  ImGui::EndMainMenuBar();
}

void Editor2DScreen::draw_toolbar() {
  if (!show_toolbar_) {
    return;
  }
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  const float bar_h = theme::metrics::kToolbarHeight;
  ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y));
  ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, bar_h));
  ImGui::SetNextWindowViewport(vp->ID);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::HeaderBg());
  ImGui::Begin("##EditorToolbar", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav);
  {
    // Copper underline separating the tool strip from the dock area.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetWindowPos();
    const ImVec2 sz = ImGui::GetWindowSize();
    dl->AddRectFilled(ImVec2(p.x, p.y + sz.y - 1.0f),
                      ImVec2(p.x + sz.x, p.y + sz.y),
                      theme::U32(theme::Copper(), 0.55f));
  }

  bool grid = workspace_.show_grid();
  if (theme::ToggleButton("Grid", &grid, ImVec2(56, 0))) {
    workspace_.set_show_grid(grid);
    mark_dirty_and_autosave();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Show grid (G)");
  }
  ImGui::SameLine();
  bool snap = workspace_.snap_enabled();
  if (theme::ToggleButton("Snap", &snap, ImVec2(56, 0))) {
    workspace_.set_snap_enabled(snap);
    mark_dirty_and_autosave();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Snap drags, nudges and Inspector X/Y/W/H to the grid (Shift+G)");
  }
  ImGui::SameLine();
  int grid_px = static_cast<int>(workspace_.grid_size() + 0.5f);
  ImGui::SetNextItemWidth(86.0f);
  if (ImGui::DragInt("##gridsize", &grid_px, 0.25f,
                     static_cast<int>(Workspace2D::kMinGrid),
                     static_cast<int>(Workspace2D::kMaxGrid), "cell %d px",
                     ImGuiSliderFlags_AlwaysClamp)) {
    workspace_.set_grid_size(static_cast<float>(grid_px));
    mark_dirty();
  }
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    mark_dirty_and_autosave();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Grid cell size. Drag or Ctrl+click to type. [ / ] halves / doubles.");
  }

  ImGui::SameLine(0.0f, 18.0f);
  const bool has_sel = workspace_.selection_count() > 0;
  ImGui::BeginDisabled(!has_sel);
  if (theme::CopperButton("Duplicate", ImVec2(84, 0))) {
    duplicate_selected();
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("Ctrl+D");
  }
  ImGui::SameLine();
  if (theme::DangerButton("Delete", ImVec2(70, 0))) {
    delete_selected();
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("Del");
  }
  ImGui::EndDisabled();

  ImGui::SameLine(0.0f, 18.0f);
  if (theme::SecondaryButton("Reset Cam", ImVec2(84, 0))) {
    workspace_.set_pan(0.0f, 0.0f);
    workspace_.set_zoom(1.0f);
  }
  ImGui::SameLine();
  if (theme::PrimaryButton("Save", ImVec2(64, 0))) {
    std::string err;
    if (save_scene(&err)) {
      note("Scene saved");
    }
  }
  ImGui::SameLine(0.0f, 18.0f);
  ImGui::TextDisabled("zoom %.0f%%", workspace_.zoom() * 100.0f);
  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(2);
}

void Editor2DScreen::draw_status_bar() {
  if (!show_status_bar_) {
    return;
  }
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  const float bar_h = theme::metrics::kStatusBarHeight;
  ImGui::SetNextWindowPos(
      ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - bar_h));
  ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, bar_h));
  ImGui::SetNextWindowViewport(vp->ID);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::HeaderBg());
  ImGui::Begin("##EditorStatusBar", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav);

  ImDrawList* dl = ImGui::GetWindowDrawList();
  {
    const ImVec2 p = ImGui::GetWindowPos();
    const float w = ImGui::GetWindowSize().x;
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + 1.5f), theme::U32(theme::Accent()));
  }
  // Brand block: headstone + wordmark.
  {
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = ImGui::GetTextLineHeight();
    theme::TombstoneMark(dl, ImVec2(p.x + 6.0f, p.y), h, theme::U32(theme::Sand()),
                         theme::U32(theme::Charcoal(), 0.85f));
    ImGui::Dummy(ImVec2(14.0f, h));
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::Accent());
    ImGui::TextUnformatted(brand::kProduct);
    ImGui::PopStyleColor();
  }

  auto sep = []() {
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
  };

  const ImGuiIO& io = ImGui::GetIO();
  const float fps = io.Framerate;
  const float ms = (fps > 0.01f) ? (1000.0f / fps) : 0.0f;
  sep();
  ImGui::Text("%.0f fps (%.1f ms)", fps, ms);
  sep();
  ImGui::Text("zoom %.0f%%", workspace_.zoom() * 100.0f);
  sep();
  if (workspace_.snap_enabled()) {
    ImGui::TextColored(theme::Accent(), "snap %.0f px", workspace_.grid_size());
  } else {
    ImGui::TextDisabled("grid %.0f px", workspace_.grid_size());
  }
  sep();
  const std::size_t count = workspace_.selection_count();
  if (const Entity2D* e = workspace_.selected()) {
    if (count > 1) {
      ImGui::Text("%zu selected (%s)", count, e->name.c_str());
    } else {
      ImGui::Text("%s @ %.0f, %.0f", e->name.c_str(), e->x, e->y);
    }
  } else {
    ImGui::TextDisabled("no selection");
  }
  const double now = ImGui::GetTime();
  if (!status_note_.empty() && now - status_note_time_ < 3.0) {
    sep();
    ImGui::TextColored(theme::Copper(), "%s", status_note_.c_str());
  }
  sep();
  ImGui::TextDisabled("%s", project_.path.c_str());
  if (dirty_) {
    ImGui::SameLine();
    ImGui::TextColored(theme::Warning(), "  unsaved");
  }

  ImGui::End();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(2);
}

void Editor2DScreen::handle_hotkeys() {
  const ImGuiIO& io = ImGui::GetIO();
  if (io.WantTextInput || renaming_) {
    return;
  }
  const bool ctrl = io.KeyCtrl || io.KeySuper;
  const bool shift = io.KeyShift;
  const bool alt = io.KeyAlt;
  const bool scene_focus = viewport_focused_ || hierarchy_focused_;

  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_D)) {
    duplicate_selected();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) &&
      workspace_.selection_count() > 0) {
    delete_selected();
  }
  if (scene_focus && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_A)) {
    workspace_.select_all();
  }
  if (scene_focus && ImGui::IsKeyPressed(ImGuiKey_Escape, false) &&
      !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    workspace_.clear_selection();
  }
  if (scene_focus && ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
    if (const Entity2D* e = workspace_.selected()) {
      show_hierarchy_ = true;
      begin_rename(e->id);
    }
  }

  if (!ctrl && !alt) {
    if (ImGui::IsKeyPressed(ImGuiKey_G, false)) {
      if (shift) {
        workspace_.set_snap_enabled(!workspace_.snap_enabled());
        note(workspace_.snap_enabled() ? "Snap on" : "Snap off");
      } else {
        workspace_.set_show_grid(!workspace_.show_grid());
      }
      mark_dirty_and_autosave();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket, false) ||
        ImGui::IsKeyPressed(ImGuiKey_RightBracket, false)) {
      const bool up = ImGui::IsKeyPressed(ImGuiKey_RightBracket, false);
      workspace_.set_grid_size(workspace_.grid_size() * (up ? 2.0f : 0.5f));
      mark_dirty_and_autosave();
      note("Grid " + std::to_string(static_cast<int>(workspace_.grid_size())) +
           " px");
    }
  }

  // Arrow nudge (with key repeat). Only while the scene panels have focus so
  // Settings-style widgets elsewhere keep normal arrow behaviour.
  if (scene_focus && !ctrl && !alt && workspace_.selection_count() > 0 &&
      drag_mode_ == DragMode::None) {
    int dx = 0;
    int dy = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) dx -= 1;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) dx += 1;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) dy -= 1;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) dy += 1;
    if (dx != 0 || dy != 0) {
      nudge_selected(dx, dy, shift);
    }
  }
  const bool arrows_held = ImGui::IsKeyDown(ImGuiKey_LeftArrow) ||
                           ImGui::IsKeyDown(ImGuiKey_RightArrow) ||
                           ImGui::IsKeyDown(ImGuiKey_UpArrow) ||
                           ImGui::IsKeyDown(ImGuiKey_DownArrow);
  if (nudge_pending_save_ && !arrows_held) {
    nudge_pending_save_ = false;
    mark_dirty_and_autosave();
  }
}

void Editor2DScreen::draw_ui() {
  draw_menu_bar();
  draw_toolbar();
  draw_status_bar();

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float top_pad = show_toolbar_ ? theme::metrics::kToolbarHeight : 0.0f;
  const float bottom_pad =
      show_status_bar_ ? theme::metrics::kStatusBarHeight : 0.0f;

  ImGui::SetNextWindowPos(
      ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + top_pad));
  ImGui::SetNextWindowSize(
      ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - top_pad - bottom_pad));
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGuiWindowFlags host_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
      ImGuiWindowFlags_NoDocking;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::Begin("##EditorDockHost", nullptr, host_flags);
  ImGui::PopStyleVar(2);

  const ImGuiID dockspace_id = ImGui::GetID("Editor2DDockspace");
  setup_default_dock_layout(dockspace_id);
  ImGui::DockSpace(dockspace_id, ImVec2(0, 0),
                   ImGuiDockNodeFlags_PassthruCentralNode);
  ImGui::End();

  viewport_focused_ = false;
  hierarchy_focused_ = false;
  // NoNavInputs on the scene panels: arrow keys nudge entities instead of
  // moving the keyboard-nav cursor.
  if (show_hierarchy_) {
    if (ImGui::Begin("Hierarchy", &show_hierarchy_,
                     ImGuiWindowFlags_NoNavInputs)) {
      draw_hierarchy();
    }
    ImGui::End();
  }
  if (show_viewport_) {
    if (ImGui::Begin("Viewport2D", &show_viewport_,
                     ImGuiWindowFlags_NoNavInputs |
                         ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoScrollWithMouse)) {
      draw_viewport();
    }
    ImGui::End();
  }
  if (show_inspector_) {
    if (ImGui::Begin("Inspector", &show_inspector_)) {
      draw_inspector();
    }
    ImGui::End();
  }

  handle_hotkeys();
}


}  // namespace editor
}  // namespace tombstone
}  // namespace ts
