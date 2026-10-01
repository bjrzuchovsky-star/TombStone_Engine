#include "editor/screens/LoadingScreen.h"

#include "editor/ui/Theme.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
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
    theme::BeginRoot("##LoadingSplash");

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::Dummy(ImVec2(0.0f, avail.y * 0.18f));

    const float card_w = std::min(avail.x * 0.55f, 520.0f);
    const float card_h = 280.0f;
    ImGui::SetCursorPosX((avail.x - card_w) * 0.5f);
    theme::BeginCard("##LoadingCard", card_w, card_h);

    const float inner_w = ImGui::GetContentRegionAvail().x;
    theme::BrandBlock("TombStone Engine", "Admin Editor", inner_w);

    ImGui::Dummy(ImVec2(0.0f, 18.0f));
    const float bar_w = std::min(inner_w, 380.0f);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (inner_w - bar_w) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, theme::Accent());
    ImGui::ProgressBar(progress, ImVec2(bar_w, 10.0f), "");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    char status[64];
    std::snprintf(status, sizeof(status), "Loading modules... %d%%",
                  static_cast<int>(progress * 100.0f));
    const ImVec2 st = ImGui::CalcTextSize(status);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (inner_w - st.x) * 0.5f);
    theme::StatusInfo(status);

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    const char* hint = "Preparing project store and editor shell";
    const ImVec2 ht = ImGui::CalcTextSize(hint);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (inner_w - ht.x) * 0.5f);
    ImGui::TextDisabled("%s", hint);

    theme::EndCard();
    theme::EndRoot();
  }

  if (elapsed_seconds_ >= kMinSplashSeconds) {
    return AppState::Login;
  }
  return AppState::Loading;
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
