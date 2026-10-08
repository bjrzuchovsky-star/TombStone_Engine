#include "core/Engine.h"
#include "editor/AppFlow.h"
#include "editor/AppState.h"
#include "editor/settings/Settings.h"
#include "editor/workspace/SceneIO.h"
#include "editor/ui/Brand.h"
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
#include <optional>
#include <string>
#include <vector>

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

  // Editor tools through the screen: Ctrl+D / Del paths must autosave.
  {
    ws2->select(std::nullopt);
    for (const editor::Entity2D& e : ws2->entities()) {
      if (e.name == "SmokePersist") {
        ws2->select(e.id);
        break;
      }
    }
    if (flow.editor_duplicate_selected() != 1) {
      std::cerr << "editor_duplicate_selected expected 1 copy\n";
      engine.shutdown();
      return 1;
    }
    editor::Workspace2D on_disk;
    std::string err;
    if (!editor::scene_io::load(on_disk, scene_file.string(), &err) ||
        on_disk.entities().size() != before_count + 2) {
      std::cerr << "Duplicate was not autosaved to scene.json: " << err << '\n';
      engine.shutdown();
      return 1;
    }
    bool copy_found = false;
    for (const editor::Entity2D& e : on_disk.entities()) {
      if (e.name == "SmokePersist 2" && e.layer == 7) {
        copy_found = true;
      }
    }
    if (!copy_found) {
      std::cerr << "Expected \"SmokePersist 2\" copy in scene.json\n";
      engine.shutdown();
      return 1;
    }
    if (flow.editor_delete_selected() != 1) {
      std::cerr << "editor_delete_selected expected 1 removal\n";
      engine.shutdown();
      return 1;
    }
    editor::Workspace2D after_delete;
    if (!editor::scene_io::load(after_delete, scene_file.string(), &err) ||
        after_delete.entities().size() != before_count + 1) {
      std::cerr << "Delete was not autosaved to scene.json\n";
      engine.shutdown();
      return 1;
    }
    std::cout << "[smoke] duplicate/delete autosave OK\n";
  }

  // Editor tools, headless: snap drag, nudge, multi-select, marquee,
  // duplicate, delete, and scene.json persistence of grid/snap/selection.
  {
    auto fail = [&](const char* what) {
      std::cerr << "Editor tools smoke failed: " << what << '\n';
      engine.shutdown();
      return 1;
    };
    editor::Workspace2D t;
    t.reset_defaults();  // Camera2D (0,0) / Player (64,64) / TileMap (0,160)
    std::uint64_t cam = 0;
    std::uint64_t player = 0;
    for (const editor::Entity2D& e : t.entities()) {
      if (e.name == "Camera2D") cam = e.id;
      if (e.name == "Player") player = e.id;
    }
    if (cam == 0 || player == 0) return fail("seed entities");

    t.set_grid_size(16.0f);
    t.set_snap_enabled(true);
    if (t.snap_value(37.0f) != 32.0f || t.snap_extent(5.0f) != 16.0f) {
      return fail("snap_value / snap_extent");
    }

    // Drag-move with snap: +13,-5 from (64,64) lands on (80,64).
    t.select(player);
    t.begin_move();
    t.update_move(13.0f, -5.0f);
    if (!t.end_move()) return fail("drag move reported no change");
    const editor::Entity2D* p = t.find(player);
    if (p->x != 80.0f || p->y != 64.0f) return fail("snapped drag position");

    // Nudge: 1 cell right, 4 cells down (Shift).
    t.nudge_selection(1, 0, false);
    t.nudge_selection(0, 1, true);
    p = t.find(player);
    if (p->x != 96.0f || p->y != 128.0f) return fail("snapped nudge");
    // Off-grid entity lands on the next grid line first.
    t.find(player)->x = 100.0f;
    t.nudge_selection(1, 0, false);
    if (t.find(player)->x != 112.0f) return fail("off-grid nudge");
    // Snap off: 1 px / 10 px steps.
    t.set_snap_enabled(false);
    t.nudge_selection(-1, 0, false);
    t.nudge_selection(-1, 0, true);
    if (t.find(player)->x != 101.0f) return fail("free nudge");
    t.set_snap_enabled(true);
    t.find(player)->x = 96.0f;

    // Multi-select (Ctrl+click semantics) + group drag keeps formation.
    t.toggle_selection(cam);
    if (t.selection_count() != 2 || t.selected_id() != cam) {
      return fail("toggle_selection");
    }
    t.begin_move();
    t.update_move(32.0f, 0.0f);
    t.end_move();
    if (t.find(cam)->x != 32.0f || t.find(player)->x != 128.0f) {
      return fail("group move");
    }
    t.toggle_selection(cam);
    if (t.selection_count() != 1 || t.selected_id() != player) {
      return fail("toggle off");
    }
    t.toggle_selection(cam);

    // Duplicate both; copies are offset one cell and become the selection.
    const std::size_t n0 = t.entities().size();
    const std::vector<std::uint64_t> copies = t.duplicate_selection();
    if (copies.size() != 2 || t.entities().size() != n0 + 2 ||
        t.selection() != copies) {
      return fail("duplicate_selection");
    }
    const editor::Entity2D* pc = t.find(t.selected_id().value_or(0));
    if (!pc || pc->name != "Camera2D 2" || pc->x != 48.0f || pc->y != 16.0f) {
      return fail("duplicate naming / offset");
    }
    t.select(copies.front());
    t.duplicate_selection();
    if (t.selected() == nullptr || t.selected()->name != "Player 3") {
      return fail("duplicate of a copy should count up (Player 3)");
    }

    // Marquee: box around the camera only.
    if (t.select_in_rect(30.0f, -4.0f, 40.0f, 8.0f, false) != 1 ||
        t.selected_id() != cam || t.selection_count() != 1) {
      return fail("select_in_rect");
    }
    t.select_in_rect(-1000.0f, -1000.0f, 1000.0f, 1000.0f, false);
    if (t.selection_count() != t.entities().size()) {
      return fail("select_in_rect all");
    }

    // Persist grid/snap/selection through scene.json.
    t.set_selection(copies, copies.back());
    const fs::path tools_dir = smoke_root / "_editor_tools";
    fs::create_directories(tools_dir);
    const std::string tools_path = (tools_dir / "scene.json").string();
    std::string err;
    if (!editor::scene_io::save(t, tools_path, &err)) return fail("save");
    editor::Workspace2D back;
    if (!editor::scene_io::load(back, tools_path, &err)) return fail("load");
    if (back.grid_size() != 16.0f || !back.snap_enabled() ||
        back.entities().size() != t.entities().size() ||
        back.selection() != copies || back.selected_id() != copies.back()) {
      return fail("grid/snap/selection roundtrip");
    }

    // Delete selection.
    const std::size_t before_del = back.entities().size();
    if (back.delete_selection() != 2 ||
        back.entities().size() != before_del - 2 ||
        back.selection_count() != 0) {
      return fail("delete_selection");
    }
    std::cout << "[smoke] editor tools OK (snap drag, nudge, multi-select, "
                 "marquee, duplicate, delete, grid/snap persistence)\n";
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
      glfwCreateWindow(1280, 720, editor::brand::kWindowTitle, nullptr, nullptr);
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
  std::string last_title = editor::brand::kWindowTitle;

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

    // Window title follows the open project ("TombStone Admin | My Claim").
    std::string title = editor::brand::kWindowTitle;
    if (flow.current_state() == AppState::Editor2D && flow.active_project()) {
      title = std::string(editor::brand::kWindowTitlePrefix) +
              flow.active_project()->name;
    }
    if (title != last_title) {
      glfwSetWindowTitle(window, title.c_str());
      last_title = title;
    }

    ImGui::Render();
    int display_w = 0;
    int display_h = 0;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    const ImVec4 clear = editor::theme::Charcoal();
    glClearColor(clear.x, clear.y, clear.z, 1.0f);
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
