#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

// Grid of tile ids owned by a TileMap entity. Origin is the entity x/y; the
// entity's w/h always equal cols * tile_size by rows * tile_size.
// Tile id 0 = empty. Ids 1..N index the tileset image (sliced left-to-right,
// top-to-bottom by tile_size) or, with no tileset, the built-in palette.
struct TileMapData {
  static constexpr int kMaxCells = 1024;  // per axis
  static constexpr int kMinTileSize = 4;
  static constexpr int kMaxTileSize = 512;
  static constexpr int kMaxTileId = 65535;

  int cols = 8;
  int rows = 2;
  int tile_size = 32;
  std::string tileset;     // project-relative image path ("" = built-in)
  std::vector<int> tiles;  // row-major, size cols * rows

  TileMapData() = default;
  TileMapData(int c, int r, int size);

  bool in_bounds(int col, int row) const {
    return col >= 0 && row >= 0 && col < cols && row < rows;
  }
  int at(int col, int row) const;           // 0 when out of bounds
  bool set(int col, int row, int tile_id);  // true if the cell changed
  // Resize keeping every tile in the overlapping top-left region.
  void resize(int new_cols, int new_rows);
  std::size_t count_nonempty() const;
  // Clamp dimensions and make tiles.size() == cols * rows.
  void normalize();

  bool operator==(const TileMapData& o) const = default;
};

// Optional sprite on any entity. Drawn as a textured quad tinted by the
// entity colour; a missing file falls back to the coloured rect.
struct SpriteData {
  std::string path;  // project-relative, e.g. "assets/rider.png"
  bool use_src_rect = false;
  int src_x = 0;
  int src_y = 0;
  int src_w = 0;  // 0 = to image edge
  int src_h = 0;
  bool flip_x = false;
  bool flip_y = false;

  bool operator==(const SpriteData& o) const = default;
};

// Built-in frontier palette used when a TileMap has no tileset image.
struct BuiltinTile {
  const char* name;
  float rgba[4];
};
inline constexpr int kBuiltinTileCount = 16;
// id in [1, kBuiltinTileCount]; other ids wrap so stale ids still draw.
const BuiltinTile& builtin_tile(int id);

namespace tile_codec {
// Compact row-major run-length text: "8*4,8*1" = eight 4s then eight 1s.
// A run of one is written without the count ("3,0,0" -> "3,2*0").
std::string encode_rle(const std::vector<int>& tiles);
// Returns false on malformed input. out is resized to `expected` cells
// (missing cells = 0, extra cells dropped).
bool decode_rle(const std::string& text, std::size_t expected,
                std::vector<int>* out);
}  // namespace tile_codec

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
