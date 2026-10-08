// ts_game: the shipped runtime. Opens a window, loads <project>/scene.json
// through runtime::World, and runs it at a fixed 60 Hz with keyboard and
// gamepad input and the scene's follow camera. No ImGui, no editor code.
//
//   ts_game --project <dir>            ride the project
//   ts_game --smoke --project <dir>    headless check (no window)

#include "GameRenderer.h"

#include "core/JsonMini.h"
#include "gfx/GlTextureUploader.h"
#include "gfx/TextureCache.h"
#include "runtime/Input.h"
#include "runtime/PlaySession.h"
#include "runtime/World.h"
#include "scene/SceneJson.h"

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#endif
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace ts::tombstone;
namespace fs = std::filesystem;
using game::GameRenderer;

constexpr const char* kUsage =
    "Usage: ts_game --project <dir> [--smoke] [--frames N]\n"
    "  --project <dir>  TombStone project folder (holds scene.json)\n"
    "  --smoke          headless check: load, ride 60 ticks, verify, exit\n"
    "  --frames N       quit after N rendered frames (testing)\n"
    "Keys: WASD / arrows move P1 | gamepads ride P1-P4 | P or F6 pause |\n"
    "      F10 step while paused | R or F5 reload scene.json | Esc quit\n";

struct Options {
  std::string project;
  bool smoke = false;
  long frames = -1;
  bool help = false;
};

bool parse_args(int argc, char** argv, Options* out, std::string* err) {
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--project" && i + 1 < argc) {
      out->project = argv[++i];
    } else if (a == "--smoke") {
      out->smoke = true;
    } else if (a == "--frames" && i + 1 < argc) {
      out->frames = std::strtol(argv[++i], nullptr, 10);
    } else if (a == "--help" || a == "-h") {
      out->help = true;
    } else {
      *err = "Unknown or incomplete argument: " + a;
      return false;
    }
  }
  return true;
}

std::string read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Display name from project.json, else the folder name.
std::string project_name(const std::string& dir) {
  const std::string text = read_file(fs::path(dir) / "project.json");
  if (auto obj = json_mini::parse_object(text)) {
    const std::string name = json_mini::get_string(*obj, "name", "");
    if (!name.empty()) {
      return name;
    }
  }
  const fs::path p = fs::path(dir).lexically_normal();
  const std::string leaf =
      (p.has_filename() ? p.filename() : p.parent_path().filename()).string();
  return leaf.empty() ? std::string("TombStone") : leaf;
}

float deadzone(float v) {
  constexpr float kDead = 0.2f;
  if (std::fabs(v) < kDead) {
    return 0.0f;
  }
  const float s = (std::fabs(v) - kDead) / (1.0f - kDead);
  return v < 0.0f ? -s : s;
}

runtime::InputFrame poll_input(GLFWwindow* win) {
  runtime::InputFrame frame;
  auto down = [win](int a, int b) {
    return glfwGetKey(win, a) == GLFW_PRESS || glfwGetKey(win, b) == GLFW_PRESS;
  };
  runtime::PlayerInput keys = runtime::digital_input(
      down(GLFW_KEY_A, GLFW_KEY_LEFT), down(GLFW_KEY_D, GLFW_KEY_RIGHT),
      down(GLFW_KEY_W, GLFW_KEY_UP), down(GLFW_KEY_S, GLFW_KEY_DOWN));
  if (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS) {
    keys.buttons |= runtime::kButtonAction;
  }
  if (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
    keys.buttons |= runtime::kButtonAlt;
  }
  if (glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_PRESS) {
    keys.buttons |= runtime::kButtonStart;
  }
  frame.slot(0) = keys;
  // Gamepads 1-4 ride slots 0-3 (slot 0 shares with the keyboard).
  for (int slot = 0; slot < kMaxPlayers; ++slot) {
    GLFWgamepadstate pad;
    if (!glfwGetGamepadState(GLFW_JOYSTICK_1 + slot, &pad)) {
      continue;
    }
    runtime::PlayerInput in;
    in.connected = true;
    in.move_x = deadzone(pad.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
    in.move_y = deadzone(pad.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
    if (pad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT]) in.move_x = -1.0f;
    if (pad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT]) in.move_x = 1.0f;
    if (pad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP]) in.move_y = -1.0f;
    if (pad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN]) in.move_y = 1.0f;
    if (pad.buttons[GLFW_GAMEPAD_BUTTON_A]) in.buttons |= runtime::kButtonAction;
    if (pad.buttons[GLFW_GAMEPAD_BUTTON_B]) in.buttons |= runtime::kButtonAlt;
    if (pad.buttons[GLFW_GAMEPAD_BUTTON_START]) {
      in.buttons |= runtime::kButtonStart;
    }
    frame.slot(slot) = runtime::merge_input(frame.slot(slot), in);
  }
  return frame;
}

// --smoke: everything the window does except the window.
int run_smoke(const std::string& dir) {
  auto fail = [](const std::string& what) {
    std::cerr << "[smoke] ts_game FAILED: " << what << '\n';
    return 1;
  };
  const fs::path scene = scene_json::scene_path_for_project(dir);
  if (!fs::exists(scene)) {
    return fail("no scene.json in " + dir);
  }
  const std::string disk_before = read_file(scene);

  runtime::PlaySession session;
  std::string err;
  if (!session.start_project(dir, &err)) {
    return fail(err);
  }
  runtime::World& world = session.world();
  world.set_view_size(1280.0f, 720.0f);
  const std::size_t actor_count = world.actors().size();
  const runtime::CameraView cam0 = world.camera();
  const runtime::Actor* rider = world.player(0);
  const float x0 = rider ? rider->data.x : 0.0f;
  const float y0 = rider ? rider->data.y : 0.0f;

  runtime::InputFrame right;
  right.slot(0).move_x = 1.0f;
  right.slot(0).connected = true;
  session.run_ticks(runtime::kTickRate, right);
  if (session.tick() != static_cast<std::uint64_t>(runtime::kTickRate)) {
    return fail("expected 60 ticks");
  }

  std::string ride = "no rider (add a Player component)";
  if (rider) {
    rider = world.player(0);
    const float want = x0 + rider->data.player->speed;
    if (std::fabs(rider->data.x - want) > 0.05f ||
        std::fabs(rider->data.y - y0) > 0.001f) {
      return fail("P1 x " + std::to_string(rider->data.x) + " want " +
                  std::to_string(want));
    }
    const runtime::CameraView cam1 = world.camera();
    const bool follows = world.camera_target() == rider->data.id;
    // A bounded camera may legitimately sit against its fence.
    const runtime::Actor* cam_actor = world.find(world.camera_entity());
    const bool bounded = cam_actor && cam_actor->data.camera &&
                         cam_actor->data.camera->use_bounds;
    const bool moved = cam1.x > cam0.x;
    if (follows && !moved && !bounded) {
      return fail("camera did not follow P1");
    }
    const char* cam_note = !follows ? "follows someone else"
                           : moved  ? "followed"
                                    : "held at its bounds";
    char buf[160];
    std::snprintf(buf, sizeof(buf), "P1 rode %.0f px in 60 ticks, camera %s",
                  static_cast<double>(rider->data.x - x0), cam_note);
    ride = buf;
  }

  // Draw list with real image sizes (decoded headless; no GL uploader).
  GameRenderer renderer(dir);
  std::vector<runtime::DrawQuad> quads;
  const runtime::CameraView cam = world.camera(session.alpha());
  world.build_draw_list(
      runtime::World::view_rect(cam, 1280.0f, 720.0f), session.alpha(),
      [&renderer](const std::string& rel, int* w, int* h) {
        return renderer.image_size(rel, w, h);
      },
      &quads);
  if (quads.empty()) {
    return fail("nothing to draw around the camera");
  }

  // Pause holds; a step is one tick.
  session.pause();
  const std::uint64_t t = session.tick();
  if (session.update(1.0, right) != 0 || !session.step_once(right) ||
      session.tick() != t + 1) {
    return fail("pause / step");
  }
  session.stop();
  if (read_file(scene) != disk_before) {
    return fail("scene.json changed (the game must never write it)");
  }
  std::cout << "[smoke] ts_game OK (\"" << project_name(dir) << "\": "
            << actor_count << " actors, " << ride << ", "
            << quads.size() << " quads, scene.json untouched)\n";
  return 0;
}

struct GameState {
  runtime::PlaySession session;
  bool reload = false;
};

void on_key(GLFWwindow* win, int key, int /*scancode*/, int action,
            int mods) {
  auto* g = static_cast<GameState*>(glfwGetWindowUserPointer(win));
  if (!g || action == GLFW_RELEASE) {
    return;
  }
  const bool repeat = action == GLFW_REPEAT;
  if (key == GLFW_KEY_ESCAPE && !repeat) {
    glfwSetWindowShouldClose(win, GLFW_TRUE);
  } else if ((key == GLFW_KEY_P || key == GLFW_KEY_F6) && !repeat) {
    g->session.toggle_pause();
  } else if (key == GLFW_KEY_F10) {
    g->session.step_once(poll_input(win));
  } else if ((key == GLFW_KEY_R || key == GLFW_KEY_F5) && !repeat &&
             (mods & GLFW_MOD_CONTROL) == 0) {
    g->reload = true;
  }
}

void on_glfw_error(int code, const char* message) {
  std::cerr << "[ts_game] GLFW error " << code << ": "
            << (message ? message : "") << '\n';
}

int run_window(const Options& opt) {
  const std::string name = project_name(opt.project);
  glfwSetErrorCallback(on_glfw_error);
  if (!glfwInit()) {
    std::cerr << "[ts_game] could not start GLFW\n";
    return 1;
  }
  // Default (compatibility) context: the renderer is fixed-function GL.
  GLFWwindow* win = glfwCreateWindow(1280, 720, (name + " | TombStone").c_str(),
                                     nullptr, nullptr);
  if (!win) {
    std::cerr << "[ts_game] could not open a window\n";
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);

  gfx::GlTextureUploader uploader;
  gfx::set_texture_uploader(&uploader);
  int code = 0;
  {
    GameRenderer renderer(opt.project);
    GameState g;
    std::string err;
    if (!g.session.start_project(opt.project, &err)) {
      std::cerr << "[ts_game] " << err << '\n';
      code = 1;
    } else {
      glfwSetWindowUserPointer(win, &g);
      glfwSetKeyCallback(win, on_key);
      std::cout << "[ts_game] riding \"" << name << "\": "
                << g.session.world().actors().size() << " actors, "
                << g.session.world().player_count() << " rider(s)\n";

      std::vector<runtime::DrawQuad> quads;
      double last = glfwGetTime();
      double title_at = last;
      long frames = 0;
      while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        if (g.reload) {
          g.reload = false;
          renderer.clear();
          if (g.session.start_project(opt.project, &err)) {
            std::cout << "[ts_game] reloaded scene.json\n";
          } else {
            std::cerr << "[ts_game] reload failed: " << err << '\n';
            break;
          }
        }
        const double now = glfwGetTime();
        g.session.update(now - last, poll_input(win));
        last = now;
        renderer.poll_changes(now);

        int ww = 0, wh = 0, fw = 0, fh = 0;
        glfwGetWindowSize(win, &ww, &wh);
        glfwGetFramebufferSize(win, &fw, &fh);
        if (ww > 0 && wh > 0 && fw > 0 && fh > 0) {
          runtime::World& world = g.session.world();
          world.set_view_size(static_cast<float>(ww), static_cast<float>(wh));
          const float alpha = g.session.alpha();
          const runtime::WorldRect view = runtime::World::view_rect(
              world.camera(alpha), static_cast<float>(ww),
              static_cast<float>(wh));
          world.build_draw_list(
              view, alpha,
              [&renderer](const std::string& rel, int* w, int* h) {
                return renderer.image_size(rel, w, h);
              },
              &quads);
          renderer.render(view, fw, fh, quads);
          glfwSwapBuffers(win);
        }
        if (now - title_at >= 0.5) {
          title_at = now;
          char title[256];
          const runtime::Actor* p1 = g.session.world().player(0);
          std::snprintf(title, sizeof(title),
                        "%s | TombStone | %s | %.0f ticks/s | P1 %.0f, %.0f",
                        name.c_str(),
                        g.session.paused() ? "HOLDING UP (P)" : "riding",
                        g.session.ticks_per_second(),
                        p1 ? static_cast<double>(p1->data.x) : 0.0,
                        p1 ? static_cast<double>(p1->data.y) : 0.0);
          glfwSetWindowTitle(win, title);
        }
        if (opt.frames >= 0 && ++frames >= opt.frames) {
          break;
        }
      }
      std::cout << "[ts_game] rode " << g.session.tick() << " ticks. So long.\n";
      glfwSetWindowUserPointer(win, nullptr);
    }
    renderer.clear();  // drop GL textures while the context lives
  }
  gfx::set_texture_uploader(nullptr);
  glfwDestroyWindow(win);
  glfwTerminate();
  return code;
}

}  // namespace

int main(int argc, char** argv) {
  Options opt;
  std::string err;
  if (!parse_args(argc, argv, &opt, &err)) {
    std::cerr << err << '\n' << kUsage;
    return 2;
  }
  if (opt.help) {
    std::cout << kUsage;
    return 0;
  }
  if (opt.project.empty()) {
    std::cerr << "ts_game needs a project.\n" << kUsage;
    return 2;
  }
  if (!fs::is_directory(opt.project)) {
    std::cerr << "No project folder at " << opt.project << '\n';
    return 2;
  }
  std::cout << "TombStone Game\n";
  return opt.smoke ? run_smoke(opt.project) : run_window(opt);
}
