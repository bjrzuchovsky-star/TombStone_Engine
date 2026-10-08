#pragma once

// The texture cache moved to gfx/TextureCache.h so ts_game shares it. The
// editor keeps using the editor:: names.
#include "gfx/TextureCache.h"

namespace ts {
namespace tombstone {
namespace editor {

using ::ts::tombstone::gfx::set_texture_uploader;
using ::ts::tombstone::gfx::TextureCache;
using ::ts::tombstone::gfx::TextureInfo;
using ::ts::tombstone::gfx::texture_uploader;
using ::ts::tombstone::gfx::TextureUploader;

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
