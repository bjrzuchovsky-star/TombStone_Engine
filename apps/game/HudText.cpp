#include "HudText.h"

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#endif
#include <GLFW/glfw3.h>

#include "stb_easy_font.h"

#include <algorithm>
#include <cstdint>

namespace ts {
namespace tombstone {
namespace game {

namespace {

constexpr int kFadeTicks = 30;
constexpr std::size_t kMaxChars = 200;

// stb_easy_font knows ' '..'~' and nothing else.
std::string printable(const std::string& text) {
  std::string out;
  out.reserve(std::min(text.size(), kMaxChars));
  for (const char ch : text) {
    if (out.size() >= kMaxChars) {
      break;
    }
    const unsigned char c = static_cast<unsigned char>(ch);
    out.push_back(c >= 32 && c <= 126 ? ch : '?');
  }
  return out;
}

void fill(float x0, float y0, float x1, float y1) {
  glBegin(GL_QUADS);
  glVertex2f(x0, y0);
  glVertex2f(x1, y0);
  glVertex2f(x1, y1);
  glVertex2f(x0, y1);
  glEnd();
}

}  // namespace

std::vector<HudToast> hud_toasts(const runtime::World& world) {
  std::vector<HudToast> out;
  const bool tag = world.player_count() > 1;
  for (const runtime::Toast& t : world.toasts()) {
    HudToast h;
    std::string text = t.text;
    if (tag && t.slot >= 0) {
      text = "P" + std::to_string(t.slot + 1) + ": " + text;
    }
    h.text = printable(text);
    const std::uint64_t left = t.until >= world.tick() ? t.until - world.tick() : 0;
    h.alpha = left >= static_cast<std::uint64_t>(kFadeTicks)
                  ? 1.0f
                  : static_cast<float>(left + 1) / (kFadeTicks + 1);
    out.push_back(std::move(h));
  }
  return out;
}

void render_toasts(const std::vector<HudToast>& toasts, int framebuffer_w,
                   int framebuffer_h) {
  if (toasts.empty() || framebuffer_w <= 0 || framebuffer_h <= 0) {
    return;
  }
  glViewport(0, 0, framebuffer_w, framebuffer_h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, framebuffer_w, framebuffer_h, 0.0, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Block letters 2x at 720p, 3x at 1080p and up.
  const float scale = std::max(2.0f, static_cast<float>(framebuffer_h / 360));
  const float pad = 5.0f * scale;
  const float line_h = 7.0f * scale;  // cap height of the font
  const float gap = 3.0f * scale;
  std::vector<char> verts;
  float bottom = static_cast<float>(framebuffer_h) - 18.0f * scale;
  // Newest at the bottom, older ones stacked above it.
  for (auto it = toasts.rbegin(); it != toasts.rend(); ++it) {
    std::string text = it->text;
    const float a = it->alpha;
    const float w = static_cast<float>(stb_easy_font_width(&text[0])) * scale;
    const float card_w = w + pad * 2.0f;
    const float card_h = line_h + pad * 2.0f;
    const float x0 = (static_cast<float>(framebuffer_w) - card_w) * 0.5f;
    const float y0 = bottom - card_h;
    if (y0 < 0.0f) {
      break;
    }
    // Saddle leather with a copper rim.
    glColor4f(0.110f, 0.086f, 0.071f, 0.90f * a);
    fill(x0, y0, x0 + card_w, y0 + card_h);
    glColor4f(0.722f, 0.384f, 0.220f, 0.95f * a);
    fill(x0, y0, x0 + card_w, y0 + scale);
    fill(x0, y0 + card_h - scale, x0 + card_w, y0 + card_h);
    // Parchment letters.
    verts.assign(text.size() * 300 + 64, 0);
    const int quads = stb_easy_font_print(0.0f, 0.0f, &text[0], nullptr,
                                          verts.data(),
                                          static_cast<int>(verts.size()));
    glPushMatrix();
    glTranslatef(x0 + pad, y0 + pad, 0.0f);
    glScalef(scale, scale, 1.0f);
    glColor4f(0.961f, 0.894f, 0.725f, a);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 16, verts.data());
    glDrawArrays(GL_QUADS, 0, quads * 4);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
    bottom = y0 - gap;
  }
}

}  // namespace game
}  // namespace tombstone
}  // namespace ts
