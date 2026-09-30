#include "editor/screens/LoginScreen.h"

#include <imgui.h>

#include <cstring>
#include <iostream>

namespace ts {
namespace tombstone {
namespace editor {

LoginScreen::LoginScreen(std::string last_username, bool auto_login_dev)
    : auto_login_dev_(auto_login_dev),
      last_username_(std::move(last_username)) {}

void LoginScreen::on_enter() {
  authenticated_ = false;
  settings_requested_ = false;
  auto_login_attempted_ = false;
  username_ = last_username_;
  error_message_.clear();
  std::memset(username_buf_, 0, sizeof(username_buf_));
  std::memset(password_buf_, 0, sizeof(password_buf_));
  if (!last_username_.empty()) {
    std::strncpy(username_buf_, last_username_.c_str(),
                 sizeof(username_buf_) - 1);
  }
  std::cout << "[Login] enter credentials, DEV_LOGIN, or open Settings\n";
}

void LoginScreen::on_exit() {
  if (authenticated_) {
    std::cout << "[Login] authenticated as \"" << username_ << "\"\n";
  }
}

void LoginScreen::draw_ui() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin("##LoginRoot", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings);

  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const float panel_w = 420.0f;
  const float panel_h = 280.0f;
  ImGui::SetCursorPos(ImVec2((avail.x - panel_w) * 0.5f,
                             (avail.y - panel_h) * 0.5f));
  ImGui::BeginChild("LoginPanel", ImVec2(panel_w, panel_h),
                    ImGuiChildFlags_Borders);

  ImGui::TextUnformatted("TombStone Admin -- Login");
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::InputText("Username", username_buf_, sizeof(username_buf_));
  ImGui::InputText("Password", password_buf_, sizeof(password_buf_),
                   ImGuiInputTextFlags_Password);

  if (!error_message_.empty()) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
    ImGui::TextWrapped("%s", error_message_.c_str());
    ImGui::PopStyleColor();
  }

  ImGui::Spacing();
  if (ImGui::Button("Login", ImVec2(120, 0))) {
    try_login(username_buf_, password_buf_);
  }
  ImGui::SameLine();
  if (ImGui::Button("Dev login", ImVec2(120, 0))) {
    submit_dev_login();
  }
  ImGui::SameLine();
  if (ImGui::Button("Settings", ImVec2(120, 0))) {
    request_settings();
  }

  ImGui::Spacing();
  ImGui::TextDisabled("Stub auth: any non-empty user/pass, or Dev login.");
  ImGui::EndChild();
  ImGui::End();
}

AppState LoginScreen::on_update(float /*delta_seconds*/) {
  if (auto_login_dev_ && !auto_login_attempted_ && !authenticated_ &&
      !settings_requested_) {
    auto_login_attempted_ = true;
    submit_dev_login();
  }

  if (ImGui::GetCurrentContext() != nullptr) {
    draw_ui();
  }

  if (authenticated_) {
    return AppState::ProjectManager;
  }
  if (settings_requested_) {
    return AppState::Settings;
  }
  return AppState::Login;
}

bool LoginScreen::try_login(const std::string& username,
                            const std::string& password) {
  if (username.empty() || password.empty()) {
    error_message_ = "Username and password must both be non-empty.";
    std::cout << "[Login] rejected: " << error_message_ << '\n';
    return false;
  }
  username_ = username;
  authenticated_ = true;
  settings_requested_ = false;
  error_message_.clear();
  std::cout << "[Login] success (stub credential check)\n";
  return true;
}

bool LoginScreen::submit_dev_login() {
  username_ = last_username_.empty() ? "dev" : last_username_;
  authenticated_ = true;
  settings_requested_ = false;
  error_message_.clear();
  std::cout << "[Login] DEV_LOGIN bypass accepted as \"" << username_
            << "\"\n";
  return true;
}

void LoginScreen::request_settings() {
  settings_requested_ = true;
  std::cout << "[Login] Settings requested\n";
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
