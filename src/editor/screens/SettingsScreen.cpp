#include "editor/screens/SettingsScreen.h"

#include <imgui.h>

#include <cstring>
#include <iostream>

namespace ts {
namespace tombstone {
namespace editor {

namespace {

int theme_to_index(const std::string& theme) {
  return (theme == "light") ? 1 : 0;
}

const char* index_to_theme(int index) {
  return index == 1 ? "light" : "dark";
}

}  // namespace

SettingsScreen::SettingsScreen(Settings settings, AppState return_state)
    : draft_(std::move(settings)), return_state_(return_state) {}

void SettingsScreen::sync_buffers_from_draft() {
  std::memset(projects_root_buf_, 0, sizeof(projects_root_buf_));
  std::memset(username_buf_, 0, sizeof(username_buf_));
  std::strncpy(projects_root_buf_, draft_.projects_root.c_str(),
               sizeof(projects_root_buf_) - 1);
  std::strncpy(username_buf_, draft_.username.c_str(),
               sizeof(username_buf_) - 1);
  theme_index_ = theme_to_index(draft_.theme);
}

void SettingsScreen::sync_draft_from_buffers() {
  draft_.projects_root = projects_root_buf_;
  draft_.username = username_buf_;
  draft_.theme = index_to_theme(theme_index_);
}

void SettingsScreen::on_enter() {
  apply_requested_ = false;
  cancel_requested_ = false;
  validation_error_.clear();
  sync_buffers_from_draft();
  std::cout << "[Settings] edit config\n";
}

void SettingsScreen::on_exit() {
  std::cout << "[Settings] leaving\n";
}

void SettingsScreen::draw_ui() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin("##SettingsRoot", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings);

  ImGui::TextUnformatted("Settings");
  ImGui::TextDisabled("config: %s",
                      SettingsStore::default_settings_path().c_str());
  ImGui::Separator();

  ImGui::InputText("projects_root", projects_root_buf_,
                   sizeof(projects_root_buf_));
  ImGui::SameLine();
  if (ImGui::Button("Browse...")) {
    validation_error_.clear();
    ImGui::OpenPopup("BrowseStub");
  }
  if (ImGui::BeginPopupModal("BrowseStub", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(
        "Folder browser is not wired yet.\n"
        "Type a cwd-relative or absolute path in projects_root.");
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::InputText("username", username_buf_, sizeof(username_buf_));
  ImGui::Checkbox("auto_login_dev", &draft_.auto_login_dev);

  const char* themes[] = {"dark", "light"};
  ImGui::Combo("theme", &theme_index_, themes, 2);

  if (!draft_.last_project_path.empty()) {
    ImGui::TextDisabled("last_project: %s", draft_.last_project_path.c_str());
  }

  if (!validation_error_.empty()) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s",
                       validation_error_.c_str());
  }

  ImGui::Spacing();
  if (ImGui::Button("Apply / Save", ImVec2(140, 0))) {
    request_apply();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel", ImVec2(120, 0))) {
    request_cancel();
  }

  ImGui::End();
}

AppState SettingsScreen::on_update(float /*delta_seconds*/) {
  if (ImGui::GetCurrentContext() != nullptr) {
    draw_ui();
  }
  if (apply_requested_ || cancel_requested_) {
    return return_state_;
  }
  return AppState::Settings;
}

void SettingsScreen::set_projects_root(std::string path) {
  draft_.projects_root = std::move(path);
  std::memset(projects_root_buf_, 0, sizeof(projects_root_buf_));
  std::strncpy(projects_root_buf_, draft_.projects_root.c_str(),
               sizeof(projects_root_buf_) - 1);
}

void SettingsScreen::set_username(std::string username) {
  draft_.username = std::move(username);
  std::memset(username_buf_, 0, sizeof(username_buf_));
  std::strncpy(username_buf_, draft_.username.c_str(),
               sizeof(username_buf_) - 1);
}

void SettingsScreen::set_auto_login_dev(bool enabled) {
  draft_.auto_login_dev = enabled;
}

void SettingsScreen::set_theme(std::string theme) {
  draft_.theme = std::move(theme);
  theme_index_ = theme_to_index(draft_.theme);
}

void SettingsScreen::request_apply() {
  sync_draft_from_buffers();
  validation_error_.clear();

  if (draft_.projects_root.empty()) {
    validation_error_ =
        "projects_root must be non-empty (example: ./TombStoneProjects).";
    apply_requested_ = false;
    return;
  }

  apply_requested_ = true;
  cancel_requested_ = false;
  std::cout << "[Settings] apply requested\n";
}

void SettingsScreen::request_cancel() {
  cancel_requested_ = true;
  apply_requested_ = false;
  validation_error_.clear();
  std::cout << "[Settings] cancel requested\n";
}

void SettingsScreen::clear_apply_request() {
  apply_requested_ = false;
}

void SettingsScreen::set_validation_error(std::string message) {
  validation_error_ = std::move(message);
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
