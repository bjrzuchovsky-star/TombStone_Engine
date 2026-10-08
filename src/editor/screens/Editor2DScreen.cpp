#include "editor/screens/Editor2DScreen.h"

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

Editor2DScreen::Editor2DScreen(ProjectInfo project)
    : project_(std::move(project)) {}

void Editor2DScreen::on_enter() {
  quit_requested_ = false;
  back_requested_ = false;
  renaming_ = false;
  panning_ = false;
  drag_mode_ = DragMode::None;
  drag_hit_id_ = 0;
  drag_additive_ = false;
  drag_moved_ = false;
  viewport_focused_ = false;
  hierarchy_focused_ = false;
  nudge_pending_save_ = false;
  has_range_anchor_ = false;
  insp_drag_slot_ = -1;
  status_note_.clear();
  dirty_ = false;
  scene_path_ = scene_io::scene_path_for_project(project_.path);

  std::string err;
  if (!project_.path.empty() && scene_io::load(workspace_, scene_path_, &err)) {
    std::cout << "[Editor2D] loaded scene.json (" << workspace_.entities().size()
              << " entities) from " << scene_path_ << '\n';
  } else {
    if (!err.empty()) {
      std::cout << "[Editor2D] no scene.json yet (" << err << ") -- seeding defaults\n";
    } else {
      std::cout << "[Editor2D] seeding default entities\n";
    }
    workspace_.reset_defaults();
    if (!project_.path.empty()) {
      std::string save_err;
      if (scene_io::save(workspace_, scene_path_, &save_err)) {
        std::cout << "[Editor2D] wrote initial scene.json to " << scene_path_
                  << '\n';
        dirty_ = false;
      } else {
        std::cout << "[Editor2D] failed to write initial scene.json: " << save_err
                  << '\n';
      }
    }
  }

  std::cout << "[Editor2D] workspace for \"" << project_.name << "\" ("
            << to_string(project_.kind) << ") path=" << project_.path << '\n';
}

void Editor2DScreen::on_exit() {
  cancel_rename();
  if (dirty_ || !project_.path.empty()) {
    std::string err;
    if (!save_scene(&err) && !err.empty()) {
      std::cout << "[Editor2D] save on leave failed: " << err << '\n';
    }
  }
  std::cout << "[Editor2D] leaving workspace\n";
}

void Editor2DScreen::mark_dirty() { dirty_ = true; }

void Editor2DScreen::mark_dirty_and_autosave() {
  dirty_ = true;
  std::string err;
  if (!save_scene(&err) && !err.empty()) {
    std::cout << "[Editor2D] autosave failed: " << err << '\n';
  }
}

bool Editor2DScreen::save_scene(std::string* error_out) {
  if (project_.path.empty()) {
    if (error_out) {
      *error_out = "Project path is empty; cannot save scene.json";
    }
    return false;
  }
  if (scene_path_.empty()) {
    scene_path_ = scene_io::scene_path_for_project(project_.path);
  }
  std::string err;
  if (!scene_io::save(workspace_, scene_path_, &err)) {
    if (error_out) {
      *error_out = err;
    }
    return false;
  }
  dirty_ = false;
  std::cout << "[Editor2D] saved scene.json (" << workspace_.entities().size()
            << " entities) -> " << scene_path_ << '\n';
  return true;
}

void Editor2DScreen::note(std::string message) {
  std::cout << "[Editor2D] " << message << '\n';
  status_note_ = std::move(message);
  status_note_time_ =
      ImGui::GetCurrentContext() != nullptr ? ImGui::GetTime() : 0.0;
}

std::size_t Editor2DScreen::duplicate_selected() {
  cancel_rename();
  const std::vector<std::uint64_t> ids = workspace_.duplicate_selection();
  if (ids.empty()) {
    return 0;
  }
  mark_dirty_and_autosave();
  note("Duplicated " + std::to_string(ids.size()) +
       (ids.size() == 1 ? " entity" : " entities"));
  return ids.size();
}

std::size_t Editor2DScreen::delete_selected() {
  cancel_rename();
  if (workspace_.move_active()) {
    workspace_.cancel_move();
    drag_mode_ = DragMode::None;
  }
  const std::size_t n = workspace_.delete_selection();
  if (n == 0) {
    return 0;
  }
  has_range_anchor_ = false;
  mark_dirty_and_autosave();
  note("Buried " + std::to_string(n) + (n == 1 ? " entity" : " entities"));
  return n;
}

bool Editor2DScreen::nudge_selected(int dir_x, int dir_y, bool large) {
  if (!workspace_.nudge_selection(dir_x, dir_y, large)) {
    return false;
  }
  mark_dirty();
  nudge_pending_save_ = true;  // flushed when the arrow keys are released
  return true;
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
    if (workspace_.rename_entity(rename_id_, std::move(name))) {
      mark_dirty_and_autosave();
    }
  }
  renaming_ = false;
}

void Editor2DScreen::cancel_rename() {
  renaming_ = false;
}

AppState Editor2DScreen::on_update(float /*delta_seconds*/) {
  if (ImGui::GetCurrentContext() != nullptr) {
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
      std::string err;
      if (!save_scene(&err) && !err.empty()) {
        std::cout << "[Editor2D] save failed: " << err << '\n';
      }
    }
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
