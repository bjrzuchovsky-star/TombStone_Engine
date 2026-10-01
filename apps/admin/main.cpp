#include "core/Engine.h"
#include "editor/AppFlow.h"
#include "editor/AppState.h"
#include "editor/settings/Settings.h"
#include "editor/workspace/SceneIO.h"
#include "editor/ui/Theme.h"
#include "editor/workspace/Workspace2D.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// GLFW will pull platform OpenGL headers (do not define GLFW_INCLUDE_NONE here).
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

using namespace ts::tombstone;
using editor::AppFlow;
using editor::AppState;
using editor::Settings;
using editor::SettingsStore;

void apply_theme(const Settings& settings) {
  editor::theme::Apply(settings.theme);
}

int run_console_smoke() {
  namespace fs = std::filesystem;

  std::cout << "TombStone Admin (--smoke console path)\n";

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

  AppFlow flow;
  flow.start();

  constexpr float kDt = 1.0f / 60.0f;

  // Loading splash is longer with ImGui; budget enough ticks.
  for (int i = 0; i < 200 && flow.current_state() == AppState::Loading; ++i) {
    engine.tick(kDt);
    flow.tick(kDt);
  }

  if (flow.current_state() != AppState::Login) {
    std::cerr << "Expected Login after Loading\n";
    engine.shutdown();
    return 1;
  }

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

  if (flow.projects().empty()) {
    std::cerr << "Expected seeded sample project on disk\n";
    engine.shutdown();
    return 1;
  }

  if (!flow.create_new_project_2d("Smoke Test 2D")) {
    std::cerr << "Failed to create 2D project: " << flow.last_error() << '\n';
    engine.shutdown();
    return 1;
  }

  // Collision / validation polish checks.
  if (flow.create_new_project_2d("Smoke Test 2D")) {
    std::cerr << "Expected collision failure for duplicate project name\n";
    engine.shutdown();
    return 1;
  }
  if (flow.create_new_project_2d("bad/name")) {
    std::cerr << "Expected validation failure for unsafe project name\n";
    engine.shutdown();
    return 1;
  }

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

  bool opened = false;
  for (std::size_t i = 0; i < flow.projects().size(); ++i) {
    if (flow.select_project(i)) {
      opened = true;
      break;
    }
  }
  if (!opened) {
    std::cerr << "Failed to open a 2D project\n";
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

  // scene.json should exist after opening Editor2D (load or seed+write).
  const std::string opened_path =
      flow.active_project() ? flow.active_project()->path : std::string{};
  if (opened_path.empty()) {
    std::cerr << "Expected active project path in Editor2D\n";
    engine.shutdown();
    return 1;
  }
  const fs::path scene_file =
      fs::path(opened_path) / "scene.json";
  if (!fs::exists(scene_file)) {
    std::cerr << "Expected scene.json after opening Editor2D: " << scene_file
              << '\n';
    engine.shutdown();
    return 1;
  }

  editor::Workspace2D* ws = flow.editor_workspace();
  if (!ws) {
    std::cerr << "Expected editor workspace pointer\n";
    engine.shutdown();
    return 1;
  }
  const std::size_t before_count = ws->entities().size();
  const std::uint64_t smoke_id = ws->create_entity("SmokePersist");
  if (smoke_id == 0) {
    std::cerr << "Failed to create SmokePersist entity\n";
    engine.shutdown();
    return 1;
  }
  if (editor::Entity2D* e = ws->find(smoke_id)) {
    e->x = 321.25f;
    e->y = 42.5f;
    e->layer = 7;
  }
  if (!flow.editor_save_scene()) {
    std::cerr << "editor_save_scene failed\n";
    engine.shutdown();
    return 1;
  }
  if (ws->entities().size() != before_count + 1) {
    std::cerr << "Expected entity count to grow after create\n";
    engine.shutdown();
    return 1;
  }

  flow.request_back_to_projects();
  if (flow.current_state() != AppState::ProjectManager) {
    std::cerr << "Expected ProjectManager after back from Editor2D\n";
    engine.shutdown();
    return 1;
  }

  // Re-open same project and verify hierarchy restored from scene.json.
  bool reopened = false;
  for (std::size_t i = 0; i < flow.projects().size(); ++i) {
    if (flow.projects()[i].path == opened_path) {
      if (flow.select_project(i)) {
        reopened = true;
      }
      break;
    }
  }
  if (!reopened || flow.current_state() != AppState::Editor2D) {
    std::cerr << "Failed to reopen project for scene.json roundtrip\n";
    engine.shutdown();
    return 1;
  }
  editor::Workspace2D* ws2 = flow.editor_workspace();
  if (!ws2) {
    std::cerr << "Expected workspace after reopen\n";
    engine.shutdown();
    return 1;
  }
  const editor::Entity2D* persisted = nullptr;
  for (const editor::Entity2D& e : ws2->entities()) {
    if (e.name == "SmokePersist") {
      persisted = &e;
      break;
    }
  }
  if (!persisted) {
    std::cerr << "SmokePersist entity missing after reload from scene.json\n";
    engine.shutdown();
    return 1;
  }
  if (persisted->x < 321.0f || persisted->x > 321.5f || persisted->layer != 7) {
    std::cerr << "SmokePersist fields not restored (x=" << persisted->x
              << " layer=" << persisted->layer << ")\n";
    engine.shutdown();
    return 1;
  }
  if (ws2->entities().size() != before_count + 1) {
    std::cerr << "Entity count mismatch after scene.json reload\n";
    engine.shutdown();
    return 1;
  }

  // Direct SceneIO roundtrip without going through GUI again.
  {
    editor::Workspace2D direct;
    direct.reset_defaults();
    const std::uint64_t id = direct.create_entity("DirectRoundTrip");
    if (editor::Entity2D* e = direct.find(id)) {
      e->w = 99.0f;
      e->color[0] = 0.11f;
    }
    const fs::path direct_dir = smoke_root / "_direct_scene_io";
    fs::create_directories(direct_dir);
    const std::string direct_path = (direct_dir / "scene.json").string();
    std::string err;
    if (!editor::scene_io::save(direct, direct_path, &err)) {
      std::cerr << "Direct scene_io::save failed: " << err << '\n';
      engine.shutdown();
      return 1;
    }
    editor::Workspace2D loaded;
    if (!editor::scene_io::load(loaded, direct_path, &err)) {
      std::cerr << "Direct scene_io::load failed: " << err << '\n';
      engine.shutdown();
      return 1;
    }
    bool found = false;
    for (const editor::Entity2D& e : loaded.entities()) {
      if (e.name == "DirectRoundTrip" && e.w > 98.5f && e.color[0] < 0.12f) {
        found = true;
        break;
      }
    }
    if (!found || loaded.entities().size() != direct.entities().size()) {
      std::cerr << "Direct SceneIO roundtrip mismatch\n";
      engine.shutdown();
      return 1;
    }
  }

  flow.request_back_to_projects();
  if (flow.current_state() != AppState::ProjectManager) {
    std::cerr << "Expected ProjectManager after second back from Editor2D\n";
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

  std::cout << "TombStone Admin smoke complete\n";
  return flow.current_state() == AppState::Quit ? 0 : 1;
}

int run_imgui_app() {
  if (!glfwInit()) {
    std::cerr << "glfwInit failed\n";
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow* window =
      glfwCreateWindow(1280, 720, "TombStone Engine - Admin", nullptr, nullptr);
  if (!window) {
    std::cerr << "glfwCreateWindow failed\n";
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  // Multi-viewport is optional; enable when the backend supports it.
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
  io.ConfigWindowsMoveFromTitleBarOnly = true;

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  Engine engine;
  if (!engine.init()) {
    std::cerr << "Engine init failed\n";
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  AppFlow flow;
  flow.start();
  apply_theme(flow.settings());
  std::string last_theme = flow.settings().theme;

  while (!glfwWindowShouldClose(window) && flow.is_running()) {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const float dt = io.DeltaTime > 0.0f ? io.DeltaTime : (1.0f / 60.0f);
    engine.tick(dt);
    flow.tick(dt);

    if (flow.settings().theme != last_theme) {
      apply_theme(flow.settings());
      last_theme = flow.settings().theme;
    }

    ImGui::Render();
    int display_w = 0;
    int display_h = 0;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      GLFWwindow* backup_current_context = glfwGetCurrentContext();
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
      glfwMakeContextCurrent(backup_current_context);
    }
    glfwSwapBuffers(window);
  }

  engine.shutdown();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  bool smoke = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--smoke") == 0) {
      smoke = true;
    }
  }

  if (smoke) {
    return run_console_smoke();
  }
  return run_imgui_app();
}
