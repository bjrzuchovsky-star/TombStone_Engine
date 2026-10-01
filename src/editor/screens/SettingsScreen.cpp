#include "editor/screens/SettingsScreen.h"

#include "editor/ui/Theme.h"

#include <imgui.h>

#include <algorithm>
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
  folder_browser_.close();
  sync_buffers_from_draft();
  std::cout << "[Settings] edit config\n";
}

void SettingsScreen::on_exit() {
  folder_browser_.close();
  std::cout << "[Settings] leaving\n";
}

void SettingsScreen::draw_ui() {
  theme::BeginRoot("##SettingsRoot");

  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const float panel_w = std::min(avail.x * 0.7f, 640.0f);
  const float panel_h = std::min(avail.y * 0.85f, 520.0f);
  ImGui::SetCursorPos(ImVec2((avail.x - panel_w) * 0.5f,
                             (avail.y - panel_h) * 0.5f));
  theme::BeginCard("##SettingsCard", panel_w, panel_h);

  theme::SectionHeader("Settings");
  ImGui::TextDisabled("config: %s",
                      SettingsStore::default_settings_path().c_str());
  ImGui::Spacing();

  theme::SectionHeader("Projects");
  ImGui::TextUnformatted("Projects root folder");
  ImGui::SetNextItemWidth(-110.0f);
  ImGui::InputText("##projects_root", projects_root_buf_,
                   sizeof(projects_root_buf_));
  ImGui::SameLine();
  if (theme::SecondaryButton("Browse...", ImVec2(100, 0))) {
    validation_error_.clear();
    folder_browser_.open(projects_root_buf_);
  }
  theme::StatusInfo(
      "Browse opens an in-app folder picker (works on all platforms).");

  if (folder_browser_.draw("Select projects_root folder")) {
    const std::string picked = folder_browser_.take_result();
    if (!picked.empty()) {
      set_projects_root(picked);
      std::cout << "[Settings] browsed projects_root -> " << picked << '\n';
    }
  }

  theme::SectionHeader("Account");
  ImGui::TextUnformatted("Default username");
  ImGui::SetNextItemWidth(-1);
  ImGui::InputText("##username", username_buf_, sizeof(username_buf_));
  ImGui::Checkbox("Auto Dev login on startup", &draft_.auto_login_dev);
  theme::StatusInfo(
      "When enabled, Login skips credentials and signs in as the saved user.");

  theme::SectionHeader("Appearance");
  const char* themes[] = {"Dark (professional)", "Light"};
  ImGui::SetNextItemWidth(280.0f);
  ImGui::Combo("##theme", &theme_index_, themes, 2);
  ImGui::SameLine();
  ImGui::TextDisabled("Theme");

  if (!draft_.last_project_path.empty()) {
    ImGui::Spacing();
    theme::SectionHeader("Session");
    ImGui::TextDisabled("Last project: %s", draft_.last_project_path.c_str());
  }

  if (!validation_error_.empty()) {
    ImGui::Spacing();
    theme::StatusError(validation_error_.c_str());
  }

  ImGui::Dummy(ImVec2(0, 16.0f));
  if (theme::PrimaryButton("Apply / Save", ImVec2(160, 34.0f))) {
    request_apply();
  }
  ImGui::SameLine();
  if (theme::SecondaryButton("Cancel", ImVec2(120, 34.0f))) {
    request_cancel();
  }

  theme::EndCard();
  theme::EndRoot();
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
