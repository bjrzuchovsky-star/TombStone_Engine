#include "editor/ui/Theme.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ts {
namespace tombstone {
namespace editor {
namespace theme {

namespace {

bool g_light = false;

ImVec4 rgba(int r, int g, int b, int a = 255) {
  return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

void apply_dark() {
  ImGuiStyle& style = ImGui::GetStyle();
  ImGui::StyleColorsDark();

  style.WindowRounding = 8.0f;
  style.ChildRounding = 6.0f;
  style.FrameRounding = 5.0f;
  style.PopupRounding = 6.0f;
  style.ScrollbarRounding = 8.0f;
  style.GrabRounding = 4.0f;
  style.TabRounding = 5.0f;
  style.WindowBorderSize = 1.0f;
  style.ChildBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.PopupBorderSize = 1.0f;
  style.WindowPadding = ImVec2(12.0f, 12.0f);
  style.FramePadding = ImVec2(10.0f, 6.0f);
  style.ItemSpacing = ImVec2(10.0f, 8.0f);
  style.ItemInnerSpacing = ImVec2(8.0f, 5.0f);
  style.IndentSpacing = 18.0f;
  style.ScrollbarSize = 12.0f;
  style.GrabMinSize = 10.0f;
  style.WindowTitleAlign = ImVec2(0.02f, 0.5f);
  style.ButtonTextAlign = ImVec2(0.5f, 0.5f);

  ImVec4* c = style.Colors;
  const ImVec4 bg = rgba(18, 20, 26);
  const ImVec4 bg2 = rgba(24, 27, 36);
  const ImVec4 bg3 = rgba(32, 36, 48);
  const ImVec4 border = rgba(48, 54, 70);
  const ImVec4 text = rgba(230, 234, 242);
  const ImVec4 text_dis = rgba(130, 138, 156);
  const ImVec4 accent = Accent();
  const ImVec4 accent_h = rgba(90, 168, 255);
  const ImVec4 accent_a = rgba(60, 140, 240);

  c[ImGuiCol_Text] = text;
  c[ImGuiCol_TextDisabled] = text_dis;
  c[ImGuiCol_WindowBg] = bg;
  c[ImGuiCol_ChildBg] = bg2;
  c[ImGuiCol_PopupBg] = rgba(22, 25, 34, 250);
  c[ImGuiCol_Border] = border;
  c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_FrameBg] = rgba(28, 32, 44);
  c[ImGuiCol_FrameBgHovered] = rgba(38, 44, 60);
  c[ImGuiCol_FrameBgActive] = rgba(48, 56, 76);
  c[ImGuiCol_TitleBg] = rgba(16, 18, 24);
  c[ImGuiCol_TitleBgActive] = rgba(22, 26, 36);
  c[ImGuiCol_TitleBgCollapsed] = rgba(16, 18, 24);
  c[ImGuiCol_MenuBarBg] = rgba(20, 22, 30);
  c[ImGuiCol_ScrollbarBg] = rgba(16, 18, 24);
  c[ImGuiCol_ScrollbarGrab] = rgba(60, 68, 88);
  c[ImGuiCol_ScrollbarGrabHovered] = rgba(80, 90, 115);
  c[ImGuiCol_ScrollbarGrabActive] = accent_a;
  c[ImGuiCol_CheckMark] = accent;
  c[ImGuiCol_SliderGrab] = accent;
  c[ImGuiCol_SliderGrabActive] = accent_h;
  c[ImGuiCol_Button] = rgba(42, 52, 74);
  c[ImGuiCol_ButtonHovered] = rgba(55, 72, 105);
  c[ImGuiCol_ButtonActive] = accent_a;
  c[ImGuiCol_Header] = rgba(45, 70, 110, 180);
  c[ImGuiCol_HeaderHovered] = rgba(55, 95, 150, 220);
  c[ImGuiCol_HeaderActive] = accent_a;
  c[ImGuiCol_Separator] = border;
  c[ImGuiCol_SeparatorHovered] = accent;
  c[ImGuiCol_SeparatorActive] = accent_h;
  c[ImGuiCol_ResizeGrip] = rgba(70, 90, 130, 100);
  c[ImGuiCol_ResizeGripHovered] = accent;
  c[ImGuiCol_ResizeGripActive] = accent_h;
  c[ImGuiCol_Tab] = bg3;
  c[ImGuiCol_TabHovered] = rgba(55, 85, 140);
  c[ImGuiCol_TabActive] = rgba(40, 65, 110);
  c[ImGuiCol_TabUnfocused] = bg2;
  c[ImGuiCol_TabUnfocusedActive] = bg3;
  c[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.45f);
  c[ImGuiCol_DockingEmptyBg] = rgba(14, 16, 22);
  c[ImGuiCol_PlotLines] = accent;
  c[ImGuiCol_PlotHistogram] = accent;
  c[ImGuiCol_TableHeaderBg] = rgba(30, 36, 50);
  c[ImGuiCol_TableBorderStrong] = border;
  c[ImGuiCol_TableBorderLight] = rgba(40, 46, 60);
  c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_TableRowBgAlt] = rgba(255, 255, 255, 8);
  c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
  c[ImGuiCol_DragDropTarget] = accent_h;
  c[ImGuiCol_NavHighlight] = accent;
  c[ImGuiCol_NavWindowingHighlight] = rgba(255, 255, 255, 180);
  c[ImGuiCol_NavWindowingDimBg] = rgba(0, 0, 0, 120);
  c[ImGuiCol_ModalWindowDimBg] = rgba(0, 0, 0, 160);
}

void apply_light() {
  ImGuiStyle& style = ImGui::GetStyle();
  ImGui::StyleColorsLight();

  style.WindowRounding = 8.0f;
  style.ChildRounding = 6.0f;
  style.FrameRounding = 5.0f;
  style.PopupRounding = 6.0f;
  style.ScrollbarRounding = 8.0f;
  style.GrabRounding = 4.0f;
  style.TabRounding = 5.0f;
  style.WindowPadding = ImVec2(12.0f, 12.0f);
  style.FramePadding = ImVec2(10.0f, 6.0f);
  style.ItemSpacing = ImVec2(10.0f, 8.0f);

  ImVec4* c = style.Colors;
  const ImVec4 accent = Accent();
  c[ImGuiCol_Button] = rgba(210, 222, 240);
  c[ImGuiCol_ButtonHovered] = rgba(180, 205, 240);
  c[ImGuiCol_ButtonActive] = rgba(90, 140, 210);
  c[ImGuiCol_Header] = rgba(180, 205, 240, 180);
  c[ImGuiCol_CheckMark] = accent;
  c[ImGuiCol_SliderGrab] = accent;
  c[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.4f);
}

}  // namespace

void Apply(const std::string& theme_name) {
  g_light = (theme_name == "light");
  if (g_light) {
    apply_light();
  } else {
    apply_dark();
  }
}

ImVec4 Accent() { return g_light ? rgba(40, 110, 200) : rgba(70, 150, 255); }
ImVec4 AccentMuted() {
  return g_light ? rgba(40, 110, 200, 140) : rgba(70, 150, 255, 140);
}
ImVec4 Danger() { return rgba(230, 80, 90); }
ImVec4 Success() { return rgba(90, 200, 120); }
ImVec4 Warning() { return rgba(230, 180, 70); }
ImVec4 PanelBg() { return g_light ? rgba(245, 247, 252) : rgba(24, 27, 36); }
ImVec4 HeaderBg() { return g_light ? rgba(230, 236, 248) : rgba(30, 34, 46); }
ImVec4 TextMuted() {
  return g_light ? rgba(90, 100, 120) : rgba(130, 138, 156);
}

bool BeginRoot(const char* id, ImGuiWindowFlags extra_flags) {
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->WorkPos);
  ImGui::SetNextWindowSize(vp->WorkSize);
  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
      extra_flags;
  return ImGui::Begin(id, nullptr, flags);
}

void EndRoot() { ImGui::End(); }

bool BeginCard(const char* id, float width, float height,
               ImGuiChildFlags child_flags) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, PanelBg());
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 18.0f));
  const bool open =
      ImGui::BeginChild(id, ImVec2(width, height), child_flags);
  return open;
}

void EndCard() {
  ImGui::EndChild();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();
}

void SectionHeader(const char* label) {
  ImGui::Spacing();
  ImGui::PushStyleColor(ImGuiCol_Text, Accent());
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const float w = ImGui::GetContentRegionAvail().x;
  ImGui::GetWindowDrawList()->AddRectFilled(
      p, ImVec2(p.x + w, p.y + 2.0f),
      ImGui::ColorConvertFloat4ToU32(AccentMuted()));
  ImGui::Dummy(ImVec2(0, 8.0f));
}

void StatusSuccess(const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, Success());
  ImGui::TextWrapped("%s", text);
  ImGui::PopStyleColor();
}

void StatusError(const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, Danger());
  ImGui::TextWrapped("%s", text);
  ImGui::PopStyleColor();
}

void StatusInfo(const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, TextMuted());
  ImGui::TextWrapped("%s", text);
  ImGui::PopStyleColor();
}

bool PrimaryButton(const char* label, const ImVec2& size) {
  ImGui::PushStyleColor(ImGuiCol_Button, Accent());
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(Accent().x * 1.1f, Accent().y * 1.1f,
                               std::min(1.0f, Accent().z * 1.15f), 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImVec4(Accent().x * 0.85f, Accent().y * 0.85f,
                               Accent().z * 0.9f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
  const bool pressed = ImGui::Button(label, size);
  ImGui::PopStyleColor(4);
  return pressed;
}

bool SecondaryButton(const char* label, const ImVec2& size) {
  return ImGui::Button(label, size);
}

bool DangerButton(const char* label, const ImVec2& size) {
  ImGui::PushStyleColor(ImGuiCol_Button, rgba(140, 40, 48));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, rgba(180, 50, 60));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, rgba(200, 60, 70));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
  const bool pressed = ImGui::Button(label, size);
  ImGui::PopStyleColor(4);
  return pressed;
}

void DimensionBadge(const char* dim) {
  const bool is_2d = (std::strcmp(dim, "2D") == 0 || std::strcmp(dim, "2d") == 0);
  const ImVec4 col = is_2d ? Accent() : Warning();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(col.x, col.y, col.z, 0.25f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x, col.y, col.z, 0.35f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(col.x, col.y, col.z, 0.35f));
  ImGui::PushStyleColor(ImGuiCol_Text, col);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 2.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
  ImGui::SmallButton(dim);
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(4);
}

void BrandBlock(const char* title, const char* subtitle, float avail_width) {
  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 cursor = ImGui::GetCursorScreenPos();
  const float logo = 48.0f;
  const float cx = cursor.x + avail_width * 0.5f;
  const ImVec2 logo_min(cx - logo * 0.5f, cursor.y);
  const ImVec2 logo_max(cx + logo * 0.5f, cursor.y + logo);
  dl->AddRectFilled(logo_min, logo_max,
                    ImGui::ColorConvertFloat4ToU32(Accent()), 10.0f);
  dl->AddText(ImVec2(cx - 10.0f, cursor.y + 12.0f), IM_COL32(255, 255, 255, 255),
              "TS");
  ImGui::Dummy(ImVec2(0, logo + 12.0f));

  const ImVec2 title_sz = ImGui::CalcTextSize(title);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail_width - title_sz.x) * 0.5f);
  ImGui::TextUnformatted(title);

  if (subtitle && subtitle[0] != '\0') {
    const ImVec2 sub_sz = ImGui::CalcTextSize(subtitle);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail_width - sub_sz.x) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, TextMuted());
    ImGui::TextUnformatted(subtitle);
    ImGui::PopStyleColor();
  }
}

}  // namespace theme
}  // namespace editor
}  // namespace tombstone
}  // namespace ts
