#pragma once

// ts_game's HUD: script toasts ("Picked up 10 gold") as small leather
// cards stacked up from the bottom of the window, lettered with
// stb_easy_font through fixed-function OpenGL (no font files, no loader).

#include "runtime/World.h"

#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace game {

struct HudToast {
  std::string text;   // printable ASCII only (anything else becomes '?')
  float alpha = 1.0f; // fades out over the toast's last half second
};

// What the shared screen shows now, oldest first: every toast in the
// world, with a "P2: " tag on per-rider ones when more than one rides.
std::vector<HudToast> hud_toasts(const runtime::World& world);

// Draws `toasts` over whatever is on the framebuffer (pixel space, y down).
void render_toasts(const std::vector<HudToast>& toasts, int framebuffer_w,
                   int framebuffer_h);

}  // namespace game
}  // namespace tombstone
}  // namespace ts
