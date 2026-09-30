#include "editor/screens/ProjectManagerScreen.h"

#include <iostream>

namespace ts {
namespace tombstone {
namespace editor {

ProjectManagerScreen::ProjectManagerScreen(std::vector<ProjectInfo> projects)
    : projects_(std::move(projects)) {}

void ProjectManagerScreen::on_enter() {
  selected_index_.reset();
  open_editor_2d_ = false;
  std::cout << "[ProjectManager] projects:\n";
  for (std::size_t i = 0; i < projects_.size(); ++i) {
    const ProjectInfo& p = projects_[i];
    std::cout << "  [" << i << "] " << p.name << " (" << to_string(p.kind)
              << ") id=" << p.id << '\n';
  }
  std::cout << "[ProjectManager] select a 2D project to open the editor stub\n";
}

void ProjectManagerScreen::on_exit() {
  if (const ProjectInfo* p = selected_project()) {
    std::cout << "[ProjectManager] opening \"" << p->name << "\"\n";
  }
}

AppState ProjectManagerScreen::on_update(float /*delta_seconds*/) {
  if (open_editor_2d_) {
    return AppState::Editor2D;
  }
  return AppState::ProjectManager;
}

bool ProjectManagerScreen::select_project(std::size_t index) {
  if (index >= projects_.size()) {
    std::cout << "[ProjectManager] invalid index " << index << '\n';
    return false;
  }

  const ProjectInfo& project = projects_[index];
  if (project.kind == ProjectKind::ThreeD) {
    std::cout << "[ProjectManager] \"" << project.name
              << "\" is 3D -- not implemented yet\n";
    return false;
  }

  selected_index_ = index;
  open_editor_2d_ = true;
  std::cout << "[ProjectManager] selected 2D project \"" << project.name
            << "\"\n";
  return true;
}

const ProjectInfo* ProjectManagerScreen::selected_project() const {
  if (!selected_index_.has_value()) {
    return nullptr;
  }
  return &projects_[*selected_index_];
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
