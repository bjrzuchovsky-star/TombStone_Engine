#include "gfx/GlTextureUploader.h"

// GLFW pulls the platform OpenGL header portably (Windows needs its
// APIENTRY / WINGDIAPI set up first; GLFW handles that).
#include <GLFW/glfw3.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F  // GL 1.2; Windows' gl.h stops at 1.1
#endif

namespace ts {
namespace tombstone {
namespace gfx {

std::uint64_t GlTextureUploader::upload_rgba(const unsigned char* pixels,
                                             int width, int height) {
  if (!pixels || width <= 0 || height <= 0) {
    return 0;
  }
  GLint prev = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);
  GLuint tex = 0;
  glGenTextures(1, &tex);
  if (tex == 0) {
    return 0;
  }
  glBindTexture(GL_TEXTURE_2D, tex);
  // Nearest: pixel art and tile slices stay crisp when zoomed.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels);
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev));
  return static_cast<std::uint64_t>(tex);
}

void GlTextureUploader::destroy(std::uint64_t handle) {
  const GLuint tex = static_cast<GLuint>(handle);
  if (tex != 0) {
    glDeleteTextures(1, &tex);
  }
}

}  // namespace gfx
}  // namespace tombstone
}  // namespace ts
