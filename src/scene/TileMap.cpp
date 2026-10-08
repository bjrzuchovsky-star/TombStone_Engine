#include "scene/TileMap.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace ts {
namespace tombstone {

namespace {

// Dusk-friendly earth tones: readable on the charcoal canvas, no neon.
constexpr BuiltinTile kBuiltin[kBuiltinTileCount] = {
    {"Dirt", {0.45f, 0.31f, 0.20f, 1.0f}, false},
    {"Sand", {0.86f, 0.74f, 0.52f, 1.0f}, false},
    {"Stone", {0.50f, 0.48f, 0.45f, 1.0f}, true},
    {"Grass", {0.42f, 0.55f, 0.27f, 1.0f}, false},
    {"Water", {0.25f, 0.47f, 0.62f, 1.0f}, true},
    {"Wood", {0.55f, 0.36f, 0.20f, 1.0f}, true},
    {"Clay", {0.72f, 0.42f, 0.28f, 1.0f}, false},
    {"Adobe", {0.80f, 0.62f, 0.45f, 1.0f}, true},
    {"Dry Brush", {0.66f, 0.60f, 0.33f, 1.0f}, false},
    {"Mud", {0.33f, 0.25f, 0.18f, 1.0f}, false},
    {"Brick", {0.62f, 0.27f, 0.20f, 1.0f}, true},
    {"Cactus", {0.30f, 0.48f, 0.30f, 1.0f}, true},
    {"Iron Rail", {0.36f, 0.37f, 0.40f, 1.0f}, false},
    {"Coal", {0.16f, 0.15f, 0.15f, 1.0f}, true},
    {"Gold Ore", {0.85f, 0.66f, 0.22f, 1.0f}, true},
    {"Bone", {0.90f, 0.86f, 0.76f, 1.0f}, false},
};

}  // namespace

int builtin_tile_index(int id) {
  if (id < 1) {
    id = 1;
  }
  return (id - 1) % kBuiltinTileCount + 1;
}

const BuiltinTile& builtin_tile(int id) {
  return kBuiltin[builtin_tile_index(id) - 1];
}

// --- Tile solidity --------------------------------------------------------------

bool SolidTable::operator()(int tile_id) const {
  if (tile_id <= 0) {
    return false;
  }
  if (builtin_) {
    tile_id = builtin_tile_index(tile_id);
  }
  const std::size_t i = static_cast<std::size_t>(tile_id);
  return i < flags_.size() && flags_[i] != 0;
}

bool SolidTable::any() const {
  return std::find(flags_.begin(), flags_.end(), char{1}) != flags_.end();
}

std::vector<int> TileSolidity::default_solid_ids(const std::string& tileset) {
  std::vector<int> ids;
  if (!tileset.empty()) {
    return ids;  // image tilesets start fully walkable
  }
  for (int id = 1; id <= kBuiltinTileCount; ++id) {
    if (kBuiltin[id - 1].solid) {
      ids.push_back(id);
    }
  }
  return ids;
}

std::vector<int> TileSolidity::solid_ids(const std::string& tileset) const {
  const auto it = overrides.find(tileset);
  return it != overrides.end() ? it->second : default_solid_ids(tileset);
}

bool TileSolidity::solid(const std::string& tileset, int tile_id) const {
  if (tile_id <= 0) {
    return false;
  }
  if (tileset.empty()) {
    tile_id = builtin_tile_index(tile_id);
  }
  const std::vector<int> ids = solid_ids(tileset);
  return std::binary_search(ids.begin(), ids.end(), tile_id);
}

bool TileSolidity::set_solid(const std::string& tileset, int tile_id,
                             bool make_solid) {
  if (tile_id <= 0 || tile_id > TileMapData::kMaxTileId) {
    return false;
  }
  if (tileset.empty()) {
    tile_id = builtin_tile_index(tile_id);
  }
  std::vector<int> ids = solid_ids(tileset);
  const auto at = std::lower_bound(ids.begin(), ids.end(), tile_id);
  const bool was = at != ids.end() && *at == tile_id;
  if (was == make_solid) {
    return false;
  }
  if (make_solid) {
    ids.insert(at, tile_id);
  } else {
    ids.erase(at);
  }
  if (ids == default_solid_ids(tileset)) {
    overrides.erase(tileset);
  } else {
    overrides[tileset] = std::move(ids);
  }
  return true;
}

SolidTable TileSolidity::table(const std::string& tileset) const {
  SolidTable t;
  t.builtin_ = tileset.empty();
  for (int id : solid_ids(tileset)) {
    if (id <= 0) {
      continue;
    }
    const std::size_t i = static_cast<std::size_t>(id);
    if (t.flags_.size() <= i) {
      t.flags_.resize(i + 1, 0);
    }
    t.flags_[i] = 1;
  }
  return t;
}

void TileSolidity::normalize() {
  for (auto it = overrides.begin(); it != overrides.end();) {
    std::vector<int>& ids = it->second;
    if (it->first.empty()) {
      for (int& id : ids) {
        id = id > 0 ? builtin_tile_index(id) : 0;
      }
    }
    ids.erase(std::remove_if(ids.begin(), ids.end(),
                             [](int id) {
                               return id <= 0 || id > TileMapData::kMaxTileId;
                             }),
              ids.end());
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    if (ids == default_solid_ids(it->first)) {
      it = overrides.erase(it);
    } else {
      ++it;
    }
  }
}

TileMapData::TileMapData(int c, int r, int size)
    : cols(c), rows(r), tile_size(size) {
  normalize();
}

int TileMapData::at(int col, int row) const {
  if (!in_bounds(col, row)) {
    return 0;
  }
  const std::size_t i =
      static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) +
      static_cast<std::size_t>(col);
  return i < tiles.size() ? tiles[i] : 0;
}

bool TileMapData::set(int col, int row, int tile_id) {
  if (!in_bounds(col, row)) {
    return false;
  }
  tile_id = std::clamp(tile_id, 0, kMaxTileId);
  const std::size_t i =
      static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) +
      static_cast<std::size_t>(col);
  if (i >= tiles.size() || tiles[i] == tile_id) {
    return false;
  }
  tiles[i] = tile_id;
  return true;
}

void TileMapData::resize(int new_cols, int new_rows) {
  new_cols = std::clamp(new_cols, 1, kMaxCells);
  new_rows = std::clamp(new_rows, 1, kMaxCells);
  if (new_cols == cols && new_rows == rows &&
      tiles.size() == static_cast<std::size_t>(cols) * rows) {
    return;
  }
  std::vector<int> next(static_cast<std::size_t>(new_cols) * new_rows, 0);
  const int keep_c = std::min(cols, new_cols);
  const int keep_r = std::min(rows, new_rows);
  for (int r = 0; r < keep_r; ++r) {
    for (int c = 0; c < keep_c; ++c) {
      next[static_cast<std::size_t>(r) * new_cols + c] = at(c, r);
    }
  }
  cols = new_cols;
  rows = new_rows;
  tiles = std::move(next);
}

std::size_t TileMapData::count_nonempty() const {
  return static_cast<std::size_t>(
      std::count_if(tiles.begin(), tiles.end(), [](int t) { return t != 0; }));
}

void TileMapData::normalize() {
  cols = std::clamp(cols, 1, kMaxCells);
  rows = std::clamp(rows, 1, kMaxCells);
  tile_size = std::clamp(tile_size, kMinTileSize, kMaxTileSize);
  tiles.resize(static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows),
               0);
  for (int& t : tiles) {
    t = std::clamp(t, 0, kMaxTileId);
  }
}

int tileset_tile_count(int tile_size, int image_w, int image_h) {
  if (tile_size <= 0 || image_w < tile_size || image_h < tile_size) {
    return 0;
  }
  return std::min((image_w / tile_size) * (image_h / tile_size),
                  TileMapData::kMaxTileId);
}

bool tileset_uv(int id, int tile_size, int image_w, int image_h, float* u0,
                float* v0, float* u1, float* v1) {
  if (id <= 0 || id > tileset_tile_count(tile_size, image_w, image_h)) {
    return false;
  }
  const int per_row = image_w / tile_size;
  const int k = id - 1;
  const float tw = static_cast<float>(image_w);
  const float th = static_cast<float>(image_h);
  *u0 = static_cast<float>((k % per_row) * tile_size) / tw;
  *v0 = static_cast<float>((k / per_row) * tile_size) / th;
  *u1 = *u0 + static_cast<float>(tile_size) / tw;
  *v1 = *v0 + static_cast<float>(tile_size) / th;
  return true;
}

namespace tile_codec {

std::string encode_rle(const std::vector<int>& tiles) {
  std::string out;
  std::size_t i = 0;
  while (i < tiles.size()) {
    std::size_t run = 1;
    while (i + run < tiles.size() && tiles[i + run] == tiles[i]) {
      ++run;
    }
    if (!out.empty()) {
      out.push_back(',');
    }
    if (run > 1) {
      out += std::to_string(run);
      out.push_back('*');
    }
    out += std::to_string(tiles[i]);
    i += run;
  }
  return out;
}

bool decode_rle(const std::string& text, std::size_t expected,
                std::vector<int>* out) {
  if (!out) {
    return false;
  }
  out->clear();
  out->reserve(expected);
  std::string_view s(text);
  std::size_t i = 0;
  auto read_uint = [&](long long* v) {
    while (i < s.size() && s[i] == ' ') ++i;
    const std::size_t start = i;
    long long acc = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
      acc = acc * 10 + (s[i] - '0');
      if (acc > 100000000LL) return false;
      ++i;
    }
    while (i < s.size() && s[i] == ' ') ++i;
    *v = acc;
    return i > start;
  };
  bool ok = true;
  while (i < s.size()) {
    long long a = 0;
    if (!read_uint(&a)) {
      ok = false;
      break;
    }
    long long count = 1;
    long long id = a;
    if (i < s.size() && s[i] == '*') {
      ++i;
      count = a;
      if (!read_uint(&id)) {
        ok = false;
        break;
      }
    }
    id = std::min<long long>(id, TileMapData::kMaxTileId);
    for (long long k = 0; k < count && out->size() < expected; ++k) {
      out->push_back(static_cast<int>(id));
    }
    if (i < s.size()) {
      if (s[i] != ',') {
        ok = false;
        break;
      }
      ++i;
    }
  }
  out->resize(expected, 0);
  return ok;
}

}  // namespace tile_codec

}  // namespace tombstone
}  // namespace ts
