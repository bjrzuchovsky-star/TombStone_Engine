#include "editor/screens/LoadingScreen.h"

#include <imgui.h>

#include <algorithm>
#include <iostream>

namespace ts {
namespace tombstone {
namespace editor {

void LoadingScreen::on_enter() {
  elapsed_seconds_ = 0.0f;
  std::cout << "[Loading] splash / asset warm-up\n";
}

void LoadingScreen::on_exit() {
  std::cout << "[Loading] done\n";
}

AppState LoadingScreen::on_update(float delta_seconds) {
  elapsed_seconds_ += delta_seconds;
  const float progress =
      std::clamp(elapsed_seconds_ / kMinSplashSeconds, 0.0f, 1.0f);

  if (ImGui::GetCurrentContext() != nullptr) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin("##LoadingSplash", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);

  const ImVec2 avail = ImGui::GetContentRegionAvail();
  ImGui::Dummy(ImVec2(0.0f, avail.y * 0.32f));

  const char* title = "TombStone Engine";
  const ImVec2 title_size = ImGui::CalcTextSize(title);
  ImGui::SetCursorPosX((avail.x - title_size.x) * 0.5f);
  ImGui::TextUnformatted(title);

  ImGui::Spacing();
  const char* subtitle = "Admin Editor";
  const ImVec2 sub_size = ImGui::CalcTextSize(subtitle);
  ImGui::SetCursorPosX((avail.x - sub_size.x) * 0.5f);
  ImGui::TextDisabled("%s", subtitle);

  ImGui::Dummy(ImVec2(0.0f, 24.0f));
  const float bar_width = std::min(avail.x * 0.5f, 420.0f);
  ImGui::SetCursorPosX((avail.x - bar_width) * 0.5f);
  ImGui::ProgressBar(progress, ImVec2(bar_width, 0.0f), "");
  ImGui::SetCursorPosX((avail.x - bar_width) * 0.5f);
  ImGui::Text("Loading... %d%%", static_cast<int>(progress * 100.0f));

  ImGui::End();
  }

  if (elapsed_seconds_ >= kMinSplashSeconds) {
    return AppState::Login;
  }
  return AppState::Loading;
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
