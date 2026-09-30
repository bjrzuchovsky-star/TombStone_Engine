#include "editor/screens/Editor2DScreen.h"

#include <iostream>

namespace ts {
namespace tombstone {
namespace editor {

Editor2DScreen::Editor2DScreen(ProjectInfo project)
    : project_(std::move(project)) {}

void Editor2DScreen::on_enter() {
  quit_requested_ = false;
  back_requested_ = false;
  panels_dumped_ = false;
  std::cout << "[Editor2D] workspace for \"" << project_.name << "\" ("
            << to_string(project_.kind) << ") path=" << project_.path << '\n';
}

void Editor2DScreen::on_exit() {
  std::cout << "[Editor2D] leaving workspace\n";
}

AppState Editor2DScreen::on_update(float /*delta_seconds*/) {
  if (!panels_dumped_) {
    // Stub panel layout -- swap for ImGui windows later.
    std::cout << "[Editor2D] panels (stub):\n";
    std::cout << "  - Toolbar\n";
    std::cout << "  - Hierarchy\n";
    std::cout << "  - Viewport2D\n";
    std::cout << "  - Inspector\n";
    panels_dumped_ = true;
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
