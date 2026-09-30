#include "editor/screens/Editor2DScreen.h"

#include <imgui.h>

#include <iostream>

namespace ts {
namespace tombstone {
namespace editor {

Editor2DScreen::Editor2DScreen(ProjectInfo project)
    : project_(std::move(project)) {}

void Editor2DScreen::on_enter() {
  quit_requested_ = false;
  back_requested_ = false;
  std::cout << "[Editor2D] workspace for \"" << project_.name << "\" ("
            << to_string(project_.kind) << ") path=" << project_.path << '\n';
}

void Editor2DScreen::on_exit() {
  std::cout << "[Editor2D] leaving workspace\n";
}

void Editor2DScreen::draw_ui() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Back to Projects")) {
        request_back_to_projects();
      }
      if (ImGui::MenuItem("Quit")) {
        request_quit();
      }
      ImGui::EndMenu();
    }
    ImGui::TextDisabled("  |  %s (%s)", project_.name.c_str(),
                        to_string(project_.kind));
    ImGui::EndMainMenuBar();
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImVec2 work_pos = viewport->WorkPos;
  const ImVec2 work_size = viewport->WorkSize;

  const float left_w = work_size.x * 0.22f;
  const float right_w = work_size.x * 0.28f;
  const float center_w = work_size.x - left_w - right_w;

  ImGui::SetNextWindowPos(work_pos);
  ImGui::SetNextWindowSize(ImVec2(left_w, work_size.y));
  ImGui::Begin("Hierarchy", nullptr,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove);
  ImGui::TextUnformatted("Scene Hierarchy (stub)");
  ImGui::Separator();
  ImGui::BulletText("Root");
  ImGui::Indent();
  ImGui::BulletText("Camera2D");
  ImGui::BulletText("Player");
  ImGui::BulletText("TileMap");
  ImGui::Unindent();
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(work_pos.x + left_w, work_pos.y));
  ImGui::SetNextWindowSize(ImVec2(center_w, work_size.y));
  ImGui::Begin("Viewport2D", nullptr,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove);
  ImGui::Text("Viewport -- %s", project_.name.c_str());
  ImGui::TextDisabled("%s", project_.path.c_str());
  ImGui::Separator();
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  ImGui::Dummy(ImVec2(avail.x, avail.y - 30.0f));
  ImGui::TextDisabled("2D viewport placeholder (no scene render yet)");
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(work_pos.x + left_w + center_w, work_pos.y));
  ImGui::SetNextWindowSize(ImVec2(right_w, work_size.y));
  ImGui::Begin("Inspector", nullptr,
               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove);
  ImGui::TextUnformatted("Inspector (stub)");
  ImGui::Separator();
  ImGui::Text("Selection: (none)");
  ImGui::Spacing();
  if (ImGui::Button("Back to Projects", ImVec2(-1, 0))) {
    request_back_to_projects();
  }
  ImGui::End();
}

AppState Editor2DScreen::on_update(float /*delta_seconds*/) {
  if (ImGui::GetCurrentContext() != nullptr) {
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
