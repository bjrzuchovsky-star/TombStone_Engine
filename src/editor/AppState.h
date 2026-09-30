#pragma once

namespace ts {
namespace tombstone {
namespace editor {

// High-level Admin UI flow. Screens map 1:1 onto these states so ImGui
// (or another UI backend) can replace the console stubs later.
enum class AppState {
  Loading,
  Login,
  ProjectManager,
  Editor2D,
  Quit,
};

inline const char* to_string(AppState state) {
  switch (state) {
    case AppState::Loading:
      return "Loading";
    case AppState::Login:
      return "Login";
    case AppState::ProjectManager:
      return "ProjectManager";
    case AppState::Editor2D:
      return "Editor2D";
    case AppState::Quit:
      return "Quit";
  }
  return "Unknown";
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
