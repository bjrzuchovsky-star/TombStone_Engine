#include "editor/screens/ProjectManagerScreen.h"

#include <iostream>

namespace ts {
namespace tombstone {
namespace editor {

ProjectManagerScreen::ProjectManagerScreen(std::vector<ProjectInfo> projects,
                                           std::string projects_root,
                                           std::string status_message)
    : projects_(std::move(projects)),
      projects_root_(std::move(projects_root)),
      status_message_(std::move(status_message)) {}

void ProjectManagerScreen::on_enter() {
  selected_index_.reset();
  open_editor_2d_ = false;
  new_project_requested_ = false;
  pending_new_project_name_.clear();
  settings_requested_ = false;
  logout_requested_ = false;

  std::cout << "[ProjectManager] root=" << projects_root_ << '\n';
  if (!status_message_.empty()) {
    std::cout << "[ProjectManager] " << status_message_ << '\n';
  }
  if (projects_.empty()) {
    std::cout << "[ProjectManager] no projects found\n";
  } else {
    std::cout << "[ProjectManager] projects:\n";
    for (std::size_t i = 0; i < projects_.size(); ++i) {
      const ProjectInfo& p = projects_[i];
      std::cout << "  [" << i << "] " << p.name << " (" << to_string(p.kind)
                << ") path=" << p.path;
      if (!p.last_opened.empty()) {
        std::cout << " last_opened=" << p.last_opened;
      }
      std::cout << '\n';
    }
  }
  std::cout << "[ProjectManager] actions: Open | New 2D Project | Settings | "
               "Logout\n";
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
  if (settings_requested_) {
    return AppState::Settings;
  }
  if (logout_requested_) {
    return AppState::Login;
  }
  return AppState::ProjectManager;
}

void ProjectManagerScreen::set_projects(std::vector<ProjectInfo> projects) {
  projects_ = std::move(projects);
}

void ProjectManagerScreen::set_status_message(std::string message) {
  status_message_ = std::move(message);
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

void ProjectManagerScreen::request_new_project_2d(std::string name) {
  pending_new_project_name_ = std::move(name);
  new_project_requested_ = true;
  std::cout << "[ProjectManager] New 2D Project requested: \""
            << pending_new_project_name_ << "\"\n";
}

void ProjectManagerScreen::request_settings() {
  settings_requested_ = true;
  std::cout << "[ProjectManager] Settings requested\n";
}

void ProjectManagerScreen::request_logout() {
  logout_requested_ = true;
  std::cout << "[ProjectManager] Logout requested\n";
}

void ProjectManagerScreen::clear_new_project_request() {
  new_project_requested_ = false;
  pending_new_project_name_.clear();
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
