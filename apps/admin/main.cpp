#include "core/Engine.h"
#include "editor/AppFlow.h"
#include "editor/AppState.h"
#include "editor/settings/Settings.h"

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
  if (settings.theme == "light") {
    ImGui::StyleColorsLight();
  } else {
    ImGui::StyleColorsDark();
  }
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
      glfwCreateWindow(1280, 720, "TombStone Admin", nullptr, nullptr);
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
    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
