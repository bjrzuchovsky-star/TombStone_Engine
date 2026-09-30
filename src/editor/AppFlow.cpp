#include "editor/AppFlow.h"

#include "editor/screens/Editor2DScreen.h"
#include "editor/screens/LoadingScreen.h"
#include "editor/screens/LoginScreen.h"
#include "editor/screens/ProjectManagerScreen.h"

#include <iostream>
#include <utility>

namespace ts {
namespace tombstone {
namespace editor {

AppFlow::AppFlow() {
  // In-memory sample projects for the Admin project manager shell.
  sample_projects_.push_back(ProjectInfo{
      .id = "sample-2d-platformer",
      .name = "Sample 2D Platformer",
      .kind = ProjectKind::TwoD,
  });
  sample_projects_.push_back(ProjectInfo{
      .id = "sample-3d-prototype",
      .name = "Sample 3D Prototype",
      .kind = ProjectKind::ThreeD,
  });
}

AppFlow::~AppFlow() {
  if (screen_) {
    screen_->on_exit();
    screen_.reset();
  }
}

void AppFlow::start() {
  active_project_.reset();
  transition_to(AppState::Loading);
}

void AppFlow::tick(float delta_seconds) {
  if (!screen_ || state_ == AppState::Quit) {
    return;
  }

  const AppState next = screen_->on_update(delta_seconds);
  if (next != state_) {
    transition_to(next);
  }
}

bool AppFlow::try_login(const std::string& username,
                        const std::string& password) {
  auto* login = dynamic_cast<LoginScreen*>(screen_.get());
  if (!login) {
    return false;
  }
  if (!login->try_login(username, password)) {
    return false;
  }
  tick(0.0f);  // apply pending Login -> ProjectManager transition
  return true;
}

bool AppFlow::submit_dev_login() {
  auto* login = dynamic_cast<LoginScreen*>(screen_.get());
  if (!login) {
    return false;
  }
  if (!login->submit_dev_login()) {
    return false;
  }
  tick(0.0f);
  return true;
}

bool AppFlow::select_project(std::size_t index) {
  auto* pm = dynamic_cast<ProjectManagerScreen*>(screen_.get());
  if (!pm) {
    return false;
  }
  if (!pm->select_project(index)) {
    return false;
  }
  if (const ProjectInfo* selected = pm->selected_project()) {
    active_project_ = std::make_unique<ProjectInfo>(*selected);
  }
  tick(0.0f);  // apply ProjectManager -> Editor2D
  return true;
}

void AppFlow::request_quit() {
  if (auto* editor = dynamic_cast<Editor2DScreen*>(screen_.get())) {
    editor->request_quit();
    tick(0.0f);
    return;
  }
  transition_to(AppState::Quit);
}

void AppFlow::transition_to(AppState next) {
  if (screen_) {
    screen_->on_exit();
    screen_.reset();
  }

  std::cout << "[AppFlow] " << to_string(state_) << " -> " << to_string(next)
            << '\n';
  state_ = next;

  if (state_ == AppState::Quit) {
    return;
  }

  screen_ = make_screen(state_);
  if (screen_) {
    screen_->on_enter();
  }
}

std::unique_ptr<IScreen> AppFlow::make_screen(AppState state) const {
  switch (state) {
    case AppState::Loading:
      return std::make_unique<LoadingScreen>();
    case AppState::Login:
      return std::make_unique<LoginScreen>();
    case AppState::ProjectManager:
      return std::make_unique<ProjectManagerScreen>(sample_projects_);
    case AppState::Editor2D: {
      ProjectInfo project = active_project_
                                ? *active_project_
                                : ProjectInfo{.id = "unknown",
                                              .name = "Untitled 2D",
                                              .kind = ProjectKind::TwoD};
      return std::make_unique<Editor2DScreen>(std::move(project));
    }
    case AppState::Quit:
      break;
  }
  return nullptr;
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
