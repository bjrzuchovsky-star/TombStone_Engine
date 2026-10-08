#pragma once

// Scene data shared by the editor, the runtime and ts_game (no UI, no GL).

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {

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
// `solid` is the default collision flag (walls, water, cactus block riders;
// dirt, sand and grass do not).
struct BuiltinTile {
  const char* name;
  float rgba[4];
  bool solid;
};
inline constexpr int kBuiltinTileCount = 16;
// id in [1, kBuiltinTileCount]; other ids wrap so stale ids still draw.
const BuiltinTile& builtin_tile(int id);
// The id builtin_tile() actually shows for `id` (1..kBuiltinTileCount).
int builtin_tile_index(int id);

// Fast per-tileset lookup built from TileSolidity (see below).
class SolidTable {
 public:
  bool operator()(int tile_id) const;
  bool any() const;

 private:
  friend struct TileSolidity;
  bool builtin_ = true;     // ids wrap like builtin_tile()
  std::vector<char> flags_;  // index = tile id
};

// Which tile ids block movement, per tileset (scene.json v4
// "tile_solidity"). Keyed by the TileMap's tileset path; "" is the built-in
// palette. A tileset listed in `overrides` uses exactly that solid set;
// an unlisted one uses its defaults: the built-in palette's flags, or
// nothing solid for an image tileset.
struct TileSolidity {
  std::map<std::string, std::vector<int>> overrides;  // sorted, unique ids

  bool solid(const std::string& tileset, int tile_id) const;
  // Effective solid ids (sorted). Built-in ids are 1..kBuiltinTileCount.
  std::vector<int> solid_ids(const std::string& tileset) const;
  // True when the flag actually changed. An override that ends up equal to
  // the defaults is dropped, so files only carry real changes.
  bool set_solid(const std::string& tileset, int tile_id, bool solid);
  SolidTable table(const std::string& tileset) const;
  // Sort / dedupe ids, drop non-positive ids and default-equal overrides.
  void normalize();

  static std::vector<int> default_solid_ids(const std::string& tileset);

  bool operator==(const TileSolidity& o) const = default;
};

// Tileset slicing shared by the editor and the runtime: tiles are cut
// left-to-right, top-to-bottom in tile_size squares. Count is 0 when the
// image is smaller than one tile.
int tileset_tile_count(int tile_size, int image_w, int image_h);
// Normalised UVs of tile `id` (1-based). False when id is outside the set.
bool tileset_uv(int id, int tile_size, int image_w, int image_h, float* u0,
                float* v0, float* u1, float* v1);

namespace tile_codec {
// Compact row-major run-length text: "8*4,8*1" = eight 4s then eight 1s.
// A run of one is written without the count ("3,0,0" -> "3,2*0").
std::string encode_rle(const std::vector<int>& tiles);
// Returns false on malformed input. out is resized to `expected` cells
// (missing cells = 0, extra cells dropped).
bool decode_rle(const std::string& text, std::size_t expected,
                std::vector<int>* out);
}  // namespace tile_codec

}  // namespace tombstone
}  // namespace ts
