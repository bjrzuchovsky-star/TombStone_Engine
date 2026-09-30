#include "core/Engine.h"
#include "editor/AppFlow.h"
#include "editor/AppState.h"
#include "editor/settings/Settings.h"

#include <filesystem>
#include <iostream>

int main() {
  using namespace ts::tombstone;
  using editor::AppFlow;
  using editor::AppState;
  using editor::Settings;
  using editor::SettingsStore;

  namespace fs = std::filesystem;

  std::cout << "TombStone Admin\n";

  // Isolate this smoke run from any developer config on disk.
  const fs::path smoke_root = "./TombStoneProjects";
  const fs::path smoke_config = "./TombStoneConfig";
  std::error_code ec;
  fs::remove_all(smoke_root, ec);
  fs::remove_all(smoke_config, ec);

  {
    Settings seed = SettingsStore::make_defaults();
    seed.projects_root = SettingsStore::default_projects_root();
    seed.username = "dev";
    seed.auto_login_dev = false;
    seed.theme = "dark";
    if (!SettingsStore::save(seed)) {
      std::cerr << "Failed to write seed settings\n";
      return 1;
    }
  }

  Engine engine;
  if (!engine.init()) {
    std::cerr << "Engine init failed\n";
    return 1;
  }

  // Admin owns the project-manager shell:
  // Loading -> Login -> ProjectManager -> Settings / Editor2D.
  AppFlow flow;
  flow.start();

  constexpr float kDt = 1.0f / 60.0f;

  for (int i = 0; i < 60 && flow.current_state() == AppState::Loading; ++i) {
    engine.tick(kDt);
    flow.tick(kDt);
  }

  if (flow.current_state() != AppState::Login) {
    std::cerr << "Expected Login after Loading\n";
    engine.shutdown();
    return 1;
  }

  // Settings reachable from Login.
  if (!flow.open_settings_from_login()) {
    std::cerr << "Failed to open Settings from Login\n";
    engine.shutdown();
    return 1;
  }
  if (!flow.cancel_settings() || flow.current_state() != AppState::Login) {
    std::cerr << "Expected return to Login after cancel Settings\n";
    engine.shutdown();
    return 1;
  }

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

  // First run seeds one sample 2D project under projects_root.
  if (flow.projects().empty()) {
    std::cerr << "Expected seeded sample project on disk\n";
    engine.shutdown();
    return 1;
  }

  // New 2D project on disk.
  if (!flow.create_new_project_2d("Smoke Test 2D")) {
    std::cerr << "Failed to create 2D project: " << flow.last_error() << '\n';
    engine.shutdown();
    return 1;
  }

  // Settings from ProjectManager: change theme and apply (same root).
  if (!flow.open_settings_from_projects()) {
    std::cerr << "Failed to open Settings from ProjectManager\n";
    engine.shutdown();
    return 1;
  }
  if (!flow.settings_set_theme("light")) {
    std::cerr << "Failed to set theme draft\n";
    engine.shutdown();
    return 1;
  }
  if (!flow.apply_settings_draft() ||
      flow.current_state() != AppState::ProjectManager) {
    std::cerr << "Expected return to ProjectManager after apply Settings\n";
    engine.shutdown();
    return 1;
  }
  if (flow.settings().theme != "light") {
    std::cerr << "Expected theme=light after Settings apply\n";
    engine.shutdown();
    return 1;
  }

  // Open first 2D project -> Editor2D.
  std::size_t open_index = 0;
  bool opened = false;
  for (std::size_t i = 0; i < flow.projects().size(); ++i) {
    if (flow.select_project(i)) {
      open_index = i;
      opened = true;
      break;
    }
  }
  if (!opened) {
    std::cerr << "Failed to open a 2D project (tried from index 0)\n";
    engine.shutdown();
    return 1;
  }
  (void)open_index;

  if (flow.current_state() != AppState::Editor2D) {
    std::cerr << "Expected Editor2D after selecting 2D project\n";
    engine.shutdown();
    return 1;
  }

  if (flow.settings().last_project_path.empty()) {
    std::cerr << "Expected last_project_path to be remembered\n";
    engine.shutdown();
    return 1;
  }

  engine.tick(kDt);
  flow.tick(kDt);

  // Coherent shell: back to ProjectManager, then logout, then quit via login.
  flow.request_back_to_projects();
  if (flow.current_state() != AppState::ProjectManager) {
    std::cerr << "Expected ProjectManager after back from Editor2D\n";
    engine.shutdown();
    return 1;
  }

  if (!flow.logout() || flow.current_state() != AppState::Login) {
    std::cerr << "Expected Login after Logout\n";
    engine.shutdown();
    return 1;
  }

  flow.request_quit();
  engine.shutdown();

  std::cout << "TombStone Admin flow complete\n";
  std::cout << "projects_root=" << flow.settings().projects_root << '\n';
  std::cout << "config=" << SettingsStore::default_settings_path() << '\n';
  return flow.current_state() == AppState::Quit ? 0 : 1;
}
