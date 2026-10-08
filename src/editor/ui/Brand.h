#pragma once

#include <cstddef>

namespace ts {
namespace tombstone {
namespace editor {
namespace brand {

// One voice for every screen: short, dry, frontier. No exclamation marks.
inline constexpr const char* kProduct = "TOMBSTONE";
inline constexpr const char* kAdminTitle = "TombStone Admin";
inline constexpr const char* kTagline = "Four riders. One frontier. Build it right.";
// ASCII only: some Windows title bars mangle UTF-8 from GLFW.
inline constexpr const char* kWindowTitle = "TombStone Admin | Frontier Editor";
inline constexpr const char* kWindowTitlePrefix = "TombStone Admin | ";

// Loading splash status lines, picked by progress.
inline const char* LoadingLine(float progress) {
  static constexpr const char* kLines[] = {
      "Surveying the territory",
      "Laying track",
      "Loading the chambers",
      "Lighting the lanterns",
      "Opening the gates",
  };
  constexpr std::size_t kCount = sizeof(kLines) / sizeof(kLines[0]);
  if (progress < 0.0f) {
    progress = 0.0f;
  }
  std::size_t idx = static_cast<std::size_t>(progress * kCount);
  if (idx >= kCount) {
    idx = kCount - 1;
  }
  return kLines[idx];
}

}  // namespace brand
}  // namespace editor
}  // namespace tombstone
}  // namespace ts
