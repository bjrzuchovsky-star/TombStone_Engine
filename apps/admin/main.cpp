#include "core/Engine.h"
#include "editor/AppFlow.h"
#include "editor/AppState.h"

#include <iostream>

int main() {
  using namespace ts::tombstone;
  using editor::AppFlow;
  using editor::AppState;

  std::cout << "TombStone Admin\n";

  Engine engine;
  if (!engine.init()) {
    std::cerr << "Engine init failed\n";
    return 1;
  }

  // Admin owns the project-manager shell:
  // Loading -> Login -> ProjectManager -> Editor2D (for 2D projects).
  AppFlow flow;
  flow.start();

  constexpr float kDt = 1.0f / 60.0f;

  // Drive Loading until it auto-advances to Login.
  for (int i = 0; i < 60 && flow.current_state() == AppState::Loading; ++i) {
    engine.tick(kDt);
    flow.tick(kDt);
  }

  if (flow.current_state() != AppState::Login) {
    std::cerr << "Expected Login after Loading\n";
    engine.shutdown();
    return 1;
  }

  // Stub login: DEV_LOGIN bypass (any non-empty user/pass also works via
  // flow.try_login). No OAuth / backend yet -- see LoginScreen comments.
  if (!flow.submit_dev_login()) {
    std::cerr << "DEV_LOGIN failed\n";
    engine.shutdown();
    return 1;
  }

  if (flow.current_state() != AppState::ProjectManager) {
    std::cerr << "Expected ProjectManager after login\n";
    engine.shutdown();
    return 1;
  }

  // Index 1 is the sample 3D project -- should stay on ProjectManager.
  if (flow.select_project(1)) {
    std::cerr << "3D project should not open an editor yet\n";
    engine.shutdown();
    return 1;
  }

  // Index 0 is the sample 2D project -> Editor2D stub.
  if (!flow.select_project(0)) {
    std::cerr << "Failed to open sample 2D project\n";
    engine.shutdown();
    return 1;
  }

  if (flow.current_state() != AppState::Editor2D) {
    std::cerr << "Expected Editor2D after selecting 2D project\n";
    engine.shutdown();
    return 1;
  }

  engine.tick(kDt);
  flow.tick(kDt);

  flow.request_quit();
  engine.shutdown();

  std::cout << "TombStone Admin flow complete\n";
  return flow.current_state() == AppState::Quit ? 0 : 1;
}
