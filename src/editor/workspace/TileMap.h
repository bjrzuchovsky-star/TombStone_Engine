#pragma once

// TileMapData / SpriteData and the tile codec moved to scene/TileMap.h so
// the runtime and ts_game share them. The editor keeps its old names.
#include "scene/TileMap.h"

namespace ts {
namespace tombstone {
namespace editor {

using ::ts::tombstone::BuiltinTile;
using ::ts::tombstone::builtin_tile;
using ::ts::tombstone::kBuiltinTileCount;
using ::ts::tombstone::SpriteData;
using ::ts::tombstone::TileMapData;
namespace tile_codec = ::ts::tombstone::tile_codec;

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
