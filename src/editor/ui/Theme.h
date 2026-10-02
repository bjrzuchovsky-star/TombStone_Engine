#pragma once

#include <imgui.h>

#include <string>

namespace ts {
namespace tombstone {
namespace editor {
namespace theme {

// TombStone Admin look — western-frontier grit without cartoon:
// deep charcoal, warm stone/sand, amber/gold primary, rust/copper secondary.
// Call Apply once at startup and whenever Settings.theme changes.
void Apply(const std::string& theme_name);

// Palette accessors (readable contrast on charcoal / warm paper).
ImVec4 Accent();       // amber / gold
ImVec4 AccentMuted();
ImVec4 Copper();       // rust / copper secondary
ImVec4 Sand();         // warm stone / sand
ImVec4 Danger();
ImVec4 Success();
ImVec4 Warning();
ImVec4 PanelBg();
ImVec4 HeaderBg();
ImVec4 TextMuted();
ImVec4 Border();

// Full-viewport root window (no decoration). Returns true while open.
bool BeginRoot(const char* id, ImGuiWindowFlags extra_flags = 0);
void EndRoot();

// Centered card panel (child) with stone-tint border feel. Call EndCard after.
bool BeginCard(const char* id, float width, float height,
               ImGuiChildFlags child_flags = ImGuiChildFlags_Borders);
void EndCard();

// Section header with amber underline.
void SectionHeader(const char* label);

// Soft status / empty-state text helpers.
void StatusSuccess(const char* text);
void StatusError(const char* text);
void StatusInfo(const char* text);

// Primary / secondary / danger buttons with consistent sizing.
bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0));

// Dimension badge (2D / 3D) drawn inline.
void DimensionBadge(const char* dim);

// Branded title block (stone-mark logo + title + subtitle).
void BrandBlock(const char* title, const char* subtitle, float avail_width);

}  // namespace theme
}  // namespace editor
}  // namespace tombstone
}  // namespace ts
