#pragma once

#include "gfx/TextureCache.h"

#include <cstdint>

namespace ts {
namespace tombstone {
namespace gfx {

// OpenGL side of the renderer-agnostic texture cache. Only GL 1.1 entry
// points, so the stock opengl32 / libGL exports are enough (no loader).
// Register with set_texture_uploader() once a context is current and
// unregister before it goes away. Used by ts_admin and ts_game.
class GlTextureUploader final : public TextureUploader {
 public:
  std::uint64_t upload_rgba(const unsigned char* pixels, int width,
                            int height) override;
  void destroy(std::uint64_t handle) override;
};

}  // namespace gfx
}  // namespace tombstone
}  // namespace ts
