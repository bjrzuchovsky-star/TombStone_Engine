#pragma once

#include <imgui.h>

#include <string>

namespace ts {
namespace tombstone {
namespace editor {
namespace theme {

// Central Admin UI look. Call Apply once at startup and whenever Settings.theme
// changes. Designed as a dark professional engine-editor shell (not stock ImGui).
void Apply(const std::string& theme_name);

// Accent used for primary actions / badges / focus chrome.
ImVec4 Accent();
ImVec4 AccentMuted();
ImVec4 Danger();
ImVec4 Success();
ImVec4 Warning();
ImVec4 PanelBg();
ImVec4 HeaderBg();
ImVec4 TextMuted();

// Full-viewport root window (no decoration). Returns true while open.
bool BeginRoot(const char* id, ImGuiWindowFlags extra_flags = 0);
void EndRoot();

// Centered card panel (child). Call EndCard after contents.
bool BeginCard(const char* id, float width, float height,
               ImGuiChildFlags child_flags = ImGuiChildFlags_Borders);
void EndCard();

// Section header with accent underline.
void SectionHeader(const char* label);

// Soft status / empty-state text helpers.
void StatusSuccess(const char* text);
void StatusError(const char* text);
void StatusInfo(const char* text);

// Primary / secondary buttons with consistent sizing.
bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0));

// Dimension badge (2D / 3D) drawn inline.
void DimensionBadge(const char* dim);

// Draw a branded title block (logo placeholder + title + subtitle).
void BrandBlock(const char* title, const char* subtitle, float avail_width);

}  // namespace theme
}  // namespace editor
}  // namespace tombstone
}  // namespace ts
