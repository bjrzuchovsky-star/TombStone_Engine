#include "editor/screens/ProjectManagerScreen.h"

#include "editor/projects/ProjectStore.h"
#include "editor/ui/Theme.h"

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
  delete_requested_ = false;
  pending_delete_index_.reset();
  confirm_delete_open_ = false;
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

void ProjectManagerScreen::draw_project_card(std::size_t i) {
  const ProjectInfo& p = projects_[i];
  const bool highlight =
      highlighted_index_.has_value() && *highlighted_index_ == i;

  ImGui::PushID(static_cast<int>(i));
  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        highlight ? ImVec4(0.16f, 0.24f, 0.38f, 1.0f)
                                  : theme::PanelBg());
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
  ImGui::BeginChild("card", ImVec2(0, 78), ImGuiChildFlags_Borders);

  ImGui::BeginGroup();
  ImGui::TextUnformatted(p.name.c_str());
  ImGui::SameLine();
  theme::DimensionBadge(to_string(p.kind));
  ImGui::TextDisabled("%s", p.path.c_str());
  if (!p.last_opened.empty()) {
    ImGui::TextDisabled("Last opened: %s", p.last_opened.c_str());
  } else if (!p.created.empty()) {
    ImGui::TextDisabled("Created: %s", p.created.c_str());
  }
  ImGui::EndGroup();

  if (ImGui::IsWindowHovered() &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    highlighted_index_ = i;
    error_message_.clear();
  }
  if (ImGui::IsWindowHovered() &&
      ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    select_project(i);
  }

  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
  ImGui::PopID();
  ImGui::Dummy(ImVec2(0, 4.0f));
}

void ProjectManagerScreen::draw_ui() {
  theme::BeginRoot("##ProjectManagerRoot");

  theme::SectionHeader("Project Manager");
  ImGui::TextDisabled("projects_root: %s", projects_root_.c_str());
  ImGui::Spacing();

  if (!status_message_.empty()) {
    theme::StatusSuccess(status_message_.c_str());
  }
  if (!error_message_.empty()) {
    theme::StatusError(error_message_.c_str());
  }

  ImGui::BeginChild("ProjectList", ImVec2(0, -120), ImGuiChildFlags_Borders);
  if (projects_.empty()) {
    ImGui::Dummy(ImVec2(0, 40));
    const char* empty = "No projects yet";
    const ImVec2 es = ImGui::CalcTextSize(empty);
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - es.x) * 0.5f);
    ImGui::TextUnformatted(empty);
    theme::StatusInfo(
        "Create a New 2D project below to get started. A sample project is "
        "seeded automatically when the projects folder is empty.");
  } else {
    for (std::size_t i = 0; i < projects_.size(); ++i) {
      draw_project_card(i);
    }
  }
  ImGui::EndChild();

  ImGui::Separator();
  ImGui::SetNextItemWidth(280.0f);
  ImGui::InputTextWithHint("##newname", "New 2D project name", new_name_buf_,
                           sizeof(new_name_buf_));
  ImGui::SameLine();
  if (theme::PrimaryButton("New 2D", ImVec2(100, 0))) {
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
  if (theme::SecondaryButton("Open", ImVec2(90, 0))) {
    if (!highlighted_index_.has_value()) {
      error_message_ = "Select a project in the list before Open.";
    } else {
      select_project(*highlighted_index_);
    }
  }
  ImGui::SameLine();
  if (theme::DangerButton("Delete", ImVec2(90, 0))) {
    if (!highlighted_index_.has_value()) {
      error_message_ = "Select a project before Delete.";
    } else {
      confirm_delete_open_ = true;
      ImGui::OpenPopup("Confirm Delete");
    }
  }
  ImGui::SameLine();
  if (theme::SecondaryButton("Settings", ImVec2(100, 0))) {
    request_settings();
  }
  ImGui::SameLine();
  if (theme::SecondaryButton("Logout", ImVec2(90, 0))) {
    request_logout();
  }

  if (confirm_delete_open_) {
    ImGui::OpenPopup("Confirm Delete");
  }
  if (ImGui::BeginPopupModal("Confirm Delete", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    const char* name = "(none)";
    if (highlighted_index_.has_value() &&
        *highlighted_index_ < projects_.size()) {
      name = projects_[*highlighted_index_].name.c_str();
    }
    ImGui::Text("Delete project \"%s\" from disk?", name);
    ImGui::TextDisabled("This removes the project folder permanently.");
    ImGui::Spacing();
    if (theme::DangerButton("Delete", ImVec2(120, 0))) {
      request_delete_highlighted();
      confirm_delete_open_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (theme::SecondaryButton("Cancel", ImVec2(120, 0))) {
      confirm_delete_open_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  theme::EndRoot();
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

void ProjectManagerScreen::request_delete_highlighted() {
  if (!highlighted_index_.has_value()) {
    error_message_ = "Select a project before Delete.";
    return;
  }
  pending_delete_index_ = highlighted_index_;
  delete_requested_ = true;
  std::cout << "[ProjectManager] Delete requested index="
            << *pending_delete_index_ << '\n';
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

void ProjectManagerScreen::clear_delete_request() {
  delete_requested_ = false;
  pending_delete_index_.reset();
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
