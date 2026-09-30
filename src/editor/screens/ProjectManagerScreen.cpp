#include "editor/screens/ProjectManagerScreen.h"

#include "editor/projects/ProjectStore.h"

#include <imgui.h>

#include <cstdio>
#include <cstring>
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
  error_message_.clear();
  std::memset(new_name_buf_, 0, sizeof(new_name_buf_));
  std::strncpy(new_name_buf_, "New 2D Project", sizeof(new_name_buf_) - 1);

  std::cout << "[ProjectManager] root=" << projects_root_ << '\n';
  if (!status_message_.empty()) {
    std::cout << "[ProjectManager] " << status_message_ << '\n';
  }
}

void ProjectManagerScreen::on_exit() {
  if (const ProjectInfo* p = selected_project()) {
    std::cout << "[ProjectManager] opening \"" << p->name << "\"\n";
  }
}

void ProjectManagerScreen::draw_ui() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin("##ProjectManagerRoot", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings);

  ImGui::TextUnformatted("Project Manager");
  ImGui::TextDisabled("projects_root: %s", projects_root_.c_str());
  ImGui::Separator();

  if (!status_message_.empty()) {
    ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.45f, 1.0f), "%s",
                       status_message_.c_str());
  }
  if (!error_message_.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s",
                       error_message_.c_str());
  }

  ImGui::BeginChild("ProjectList", ImVec2(0, -110), ImGuiChildFlags_Borders);
  if (projects_.empty()) {
    ImGui::TextDisabled("No projects found. Create a New 2D project below.");
  } else {
    for (std::size_t i = 0; i < projects_.size(); ++i) {
      const ProjectInfo& p = projects_[i];
      const bool highlight =
          highlighted_index_.has_value() && *highlighted_index_ == i;
      if (highlight) {
        ImGui::PushStyleColor(ImGuiCol_Header,
                              ImVec4(0.2f, 0.45f, 0.75f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              ImVec4(0.25f, 0.5f, 0.8f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                              ImVec4(0.3f, 0.55f, 0.85f, 1.0f));
      }

      char label[256];
      std::snprintf(label, sizeof(label), "%s  [%s]##proj%zu", p.name.c_str(),
                    to_string(p.kind), i);
      const bool selected = highlight;
      if (ImGui::Selectable(label, selected,
                            ImGuiSelectableFlags_AllowDoubleClick)) {
        highlighted_index_ = i;
        error_message_.clear();
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          select_project(i);
        }
      }

      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("path: %s", p.path.c_str());
        if (!p.created.empty()) {
          ImGui::Text("created: %s", p.created.c_str());
        }
        if (!p.last_opened.empty()) {
          ImGui::Text("last opened: %s", p.last_opened.c_str());
        }
        ImGui::TextUnformatted("Double-click to open (2D only).");
        ImGui::EndTooltip();
      }

      if (highlight) {
        ImGui::PopStyleColor(3);
      }
    }
  }
  ImGui::EndChild();

  ImGui::Separator();
  ImGui::InputText("New 2D name", new_name_buf_, sizeof(new_name_buf_));

  if (ImGui::Button("Open", ImVec2(100, 0))) {
    if (!highlighted_index_.has_value()) {
      error_message_ = "Select a project in the list before Open.";
    } else {
      select_project(*highlighted_index_);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("New 2D", ImVec2(100, 0))) {
    std::string validate_err;
    if (!ProjectStore::validate_project_name(new_name_buf_, &validate_err)) {
      error_message_ = validate_err;
      status_message_.clear();
    } else {
      error_message_.clear();
      request_new_project_2d(new_name_buf_);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Settings", ImVec2(100, 0))) {
    request_settings();
  }
  ImGui::SameLine();
  if (ImGui::Button("Logout", ImVec2(100, 0))) {
    request_logout();
  }

  ImGui::End();
}

AppState ProjectManagerScreen::on_update(float /*delta_seconds*/) {
  if (ImGui::GetCurrentContext() != nullptr) {
    draw_ui();
  }

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

void ProjectManagerScreen::set_error_message(std::string message) {
  error_message_ = std::move(message);
}

bool ProjectManagerScreen::select_project(std::size_t index) {
  if (index >= projects_.size()) {
    error_message_ = "Invalid project index " + std::to_string(index) + ".";
    std::cout << "[ProjectManager] " << error_message_ << '\n';
    return false;
  }

  const ProjectInfo& project = projects_[index];
  if (project.kind == ProjectKind::ThreeD) {
    error_message_ = "Cannot open \"" + project.name +
                     "\": 3D projects are not implemented yet. "
                     "Choose a 2D project.";
    status_message_.clear();
    std::cout << "[ProjectManager] " << error_message_ << '\n';
    return false;
  }

  highlighted_index_ = index;
  selected_index_ = index;
  open_editor_2d_ = true;
  error_message_.clear();
  std::cout << "[ProjectManager] selected 2D project \"" << project.name
            << "\"\n";
  return true;
}

bool ProjectManagerScreen::highlight_project_by_id(const std::string& id) {
  for (std::size_t i = 0; i < projects_.size(); ++i) {
    if (projects_[i].id == id) {
      highlighted_index_ = i;
      return true;
    }
  }
  return false;
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
