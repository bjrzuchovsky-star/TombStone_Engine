#include "editor/workspace/SceneIO.h"
#include "editor/settings/JsonMini.h"
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
namespace ts {
namespace tombstone {
namespace editor {
namespace scene_io {
namespace fs = std::filesystem;
namespace {
using json_mini::escape_string;
using json_mini::match_char;
using json_mini::parse_bool;
using json_mini::parse_string;
using json_mini::skip_ws;
std::string format_number(double v) {
  std::ostringstream ss;
  ss.setf(std::ios::fmtflags(0), std::ios::floatfield);
  ss.precision(9);
  ss << v;
  std::string s = ss.str();
  if (s.find('.') != std::string::npos || s.find('e') != std::string::npos ||
      s.find('E') != std::string::npos) {
    return s;
  }
  return s;
}
std::optional<double> parse_number(std::string_view text, std::size_t& i) {
  skip_ws(text, i);
  if (i >= text.size()) {
    return std::nullopt;
  }
  const std::size_t start = i;
  if (text[i] == '-' || text[i] == '+') {
    ++i;
  }
  bool any_digit = false;
  while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
    any_digit = true;
    ++i;
  }
  if (i < text.size() && text[i] == '.') {
    ++i;
    while (i < text.size() &&
           std::isdigit(static_cast<unsigned char>(text[i]))) {
      any_digit = true;
      ++i;
    }
  }
  if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
    ++i;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
      ++i;
    }
    bool exp_digit = false;
    while (i < text.size() &&
           std::isdigit(static_cast<unsigned char>(text[i]))) {
      exp_digit = true;
      ++i;
    }
    if (!exp_digit) {
      return std::nullopt;
    }
  }
  if (!any_digit) {
    return std::nullopt;
  }
  try {
    return std::stod(std::string(text.substr(start, i - start)));
  } catch (...) {
    return std::nullopt;
  }
}
std::optional<std::unordered_map<std::string, std::string>> parse_flat_object(
    std::string_view text, std::size_t& i) {
  if (!match_char(text, i, '{')) {
    return std::nullopt;
  }
  std::unordered_map<std::string, std::string> out;
  skip_ws(text, i);
  if (match_char(text, i, '}')) {
    return out;
  }
  while (true) {
    auto key = parse_string(text, i);
    if (!key || !match_char(text, i, ':')) {
      return std::nullopt;
    }
    skip_ws(text, i);
    if (i < text.size() && text[i] == '"') {
      auto val = parse_string(text, i);
      if (!val) {
        return std::nullopt;
      }
      out.emplace(*key, *val);
    } else if (auto b = parse_bool(text, i)) {
      out.emplace(*key, *b ? "true" : "false");
    } else if (auto n = parse_number(text, i)) {
      out.emplace(*key, format_number(*n));
    } else if (i < text.size() && text[i] == '[') {
      int depth = 0;
      do {
        if (text[i] == '[') {
          ++depth;
        } else if (text[i] == ']') {
          --depth;
        }
        ++i;
      } while (i < text.size() && depth > 0);
      out.emplace(*key, "");
    } else if (i < text.size() && text[i] == '{') {
      int depth = 0;
      do {
        if (text[i] == '{') {
          ++depth;
        } else if (text[i] == '}') {
          --depth;
        }
        ++i;
      } while (i < text.size() && depth > 0);
      out.emplace(*key, "");
    } else {
      return std::nullopt;
    }
    skip_ws(text, i);
    if (match_char(text, i, '}')) {
      return out;
    }
    if (!match_char(text, i, ',')) {
      return std::nullopt;
    }
  }
}
double map_number(const std::unordered_map<std::string, std::string>& map,
                  std::string_view key, double fallback) {
  const auto it = map.find(std::string(key));
  if (it == map.end() || it->second.empty()) {
    return fallback;
  }
  try {
    return std::stod(it->second);
  } catch (...) {
    return fallback;
  }
}
std::uint64_t map_u64(const std::unordered_map<std::string, std::string>& map,
                      std::string_view key, std::uint64_t fallback) {
  const double v = map_number(map, key, static_cast<double>(fallback));
  if (v < 0.0) {
    return fallback;
  }
  return static_cast<std::uint64_t>(v + 0.5);
}
bool map_bool(const std::unordered_map<std::string, std::string>& map,
              std::string_view key, bool fallback) {
  const auto it = map.find(std::string(key));
  if (it == map.end()) {
    return fallback;
  }
  return it->second == "true" || it->second == "1";
}
std::string map_string(const std::unordered_map<std::string, std::string>& map,
                       std::string_view key, std::string_view fallback) {
  const auto it = map.find(std::string(key));
  if (it == map.end()) {
    return std::string(fallback);
  }
  return it->second;
}
Entity2D entity_from_map(
    const std::unordered_map<std::string, std::string>& map) {
  Entity2D e;
  e.id = map_u64(map, "id", 0);
  e.name = map_string(map, "name", "Entity");
  e.x = static_cast<float>(map_number(map, "x", 0.0));
  e.y = static_cast<float>(map_number(map, "y", 0.0));
  e.w = static_cast<float>(map_number(map, "w", 64.0));
  e.h = static_cast<float>(map_number(map, "h", 64.0));
  e.color[0] = static_cast<float>(
      map_number(map, "r", map_number(map, "color_r", 0.35)));
  e.color[1] = static_cast<float>(
      map_number(map, "g", map_number(map, "color_g", 0.65)));
  e.color[2] = static_cast<float>(
      map_number(map, "b", map_number(map, "color_b", 0.95)));
  e.color[3] = static_cast<float>(
      map_number(map, "a", map_number(map, "color_a", 1.0)));
  e.layer = static_cast<int>(map_number(map, "layer", 0.0));
  return e;
}
}  // namespace
bool load(Workspace2D& workspace, const std::string& scene_path,
          std::string* error_out) {
  if (scene_path.empty()) {
    if (error_out) {
      *error_out = "scene path is empty";
    }
    return false;
  }
  if (!fs::exists(scene_path)) {
    if (error_out) {
      *error_out = "Missing scene.json: " + scene_path;
    }
    return false;
  }
  std::ifstream in(scene_path);
  if (!in) {
    if (error_out) {
      *error_out = "Could not open " + scene_path;
    }
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  const std::string text = ss.str();
  std::size_t i = 0;
  if (!match_char(text, i, '{')) {
    if (error_out) {
      *error_out = "Invalid scene.json (expected object): " + scene_path;
    }
    return false;
  }
  float pan_x = 0.0f;
  float pan_y = 0.0f;
  float zoom = 1.0f;
  bool show_grid = true;
  std::optional<float> grid_size;
  std::optional<bool> snap;
  std::vector<std::uint64_t> selection;
  std::optional<std::uint64_t> selected_id;
  std::vector<Entity2D> entities;
  bool saw_entities = false;
  skip_ws(text, i);
  if (match_char(text, i, '}')) {
    workspace.replace_scene(std::move(entities), selected_id, pan_x, pan_y,
                            zoom, show_grid);
    return true;
  }
  while (true) {
    auto key = parse_string(text, i);
    if (!key || !match_char(text, i, ':')) {
      if (error_out) {
        *error_out = "Invalid scene.json (bad key): " + scene_path;
      }
      return false;
    }
    skip_ws(text, i);
    if (*key == "entities") {
      if (!match_char(text, i, '[')) {
        if (error_out) {
          *error_out = "Invalid scene.json (entities must be array): " +
                       scene_path;
        }
        return false;
      }
      saw_entities = true;
      skip_ws(text, i);
      if (!match_char(text, i, ']')) {
        while (true) {
          auto obj = parse_flat_object(text, i);
          if (!obj) {
            if (error_out) {
              *error_out =
                  "Invalid scene.json (bad entity object): " + scene_path;
            }
            return false;
          }
          Entity2D e = entity_from_map(*obj);
          if (e.id == 0 || e.name.empty()) {
            if (error_out) {
              *error_out =
                  "Invalid scene.json (entity missing id/name): " + scene_path;
            }
            return false;
          }
          entities.push_back(std::move(e));
          skip_ws(text, i);
          if (match_char(text, i, ']')) {
            break;
          }
          if (!match_char(text, i, ',')) {
            if (error_out) {
              *error_out =
                  "Invalid scene.json (entities array): " + scene_path;
            }
            return false;
          }
        }
      }
    } else if (*key == "pan_x") {
      auto n = parse_number(text, i);
      if (!n) {
        if (error_out) {
          *error_out = "Invalid scene.json (pan_x): " + scene_path;
        }
        return false;
      }
      pan_x = static_cast<float>(*n);
    } else if (*key == "pan_y") {
      auto n = parse_number(text, i);
      if (!n) {
        if (error_out) {
          *error_out = "Invalid scene.json (pan_y): " + scene_path;
        }
        return false;
      }
      pan_y = static_cast<float>(*n);
    } else if (*key == "zoom") {
      auto n = parse_number(text, i);
      if (!n) {
        if (error_out) {
          *error_out = "Invalid scene.json (zoom): " + scene_path;
        }
        return false;
      }
      zoom = static_cast<float>(*n);
    } else if (*key == "show_grid") {
      auto b = parse_bool(text, i);
      if (!b) {
        if (error_out) {
          *error_out = "Invalid scene.json (show_grid): " + scene_path;
        }
        return false;
      }
      show_grid = *b;
    } else if (*key == "grid_size") {
      auto n = parse_number(text, i);
      if (!n) {
        if (error_out) {
          *error_out = "Invalid scene.json (grid_size): " + scene_path;
        }
        return false;
      }
      grid_size = static_cast<float>(*n);
    } else if (*key == "snap") {
      auto b = parse_bool(text, i);
      if (!b) {
        if (error_out) {
          *error_out = "Invalid scene.json (snap): " + scene_path;
        }
        return false;
      }
      snap = *b;
    } else if (*key == "selection") {
      if (!match_char(text, i, '[')) {
        if (error_out) {
          *error_out = "Invalid scene.json (selection must be array): " +
                       scene_path;
        }
        return false;
      }
      skip_ws(text, i);
      if (!match_char(text, i, ']')) {
        while (true) {
          skip_ws(text, i);
          auto n = parse_number(text, i);
          if (!n || *n < 0.0) {
            if (error_out) {
              *error_out = "Invalid scene.json (selection): " + scene_path;
            }
            return false;
          }
          selection.push_back(static_cast<std::uint64_t>(*n + 0.5));
          skip_ws(text, i);
          if (match_char(text, i, ']')) {
            break;
          }
          if (!match_char(text, i, ',')) {
            if (error_out) {
              *error_out = "Invalid scene.json (selection): " + scene_path;
            }
            return false;
          }
        }
      }
    } else if (*key == "selected_id") {
      skip_ws(text, i);
      if (i < text.size() && text[i] == 'n') {
        if (text.substr(i, 4) == "null") {
          i += 4;
          selected_id.reset();
        } else {
          if (error_out) {
            *error_out = "Invalid scene.json (selected_id): " + scene_path;
          }
          return false;
        }
      } else {
        auto n = parse_number(text, i);
        if (!n) {
          if (error_out) {
            *error_out = "Invalid scene.json (selected_id): " + scene_path;
          }
          return false;
        }
        selected_id = static_cast<std::uint64_t>(*n + 0.5);
      }
    } else if (*key == "version") {
      if (!parse_number(text, i)) {
        if (error_out) {
          *error_out = "Invalid scene.json (version): " + scene_path;
        }
        return false;
      }
    } else {
      skip_ws(text, i);
      if (i < text.size() && text[i] == '"') {
        if (!parse_string(text, i)) {
          return false;
        }
      } else if (parse_bool(text, i)) {
      } else if (parse_number(text, i)) {
      } else if (i < text.size() && text[i] == '[') {
        int depth = 0;
        do {
          if (text[i] == '[') {
            ++depth;
          } else if (text[i] == ']') {
            --depth;
          }
          ++i;
        } while (i < text.size() && depth > 0);
      } else if (i < text.size() && text[i] == '{') {
        int depth = 0;
        do {
          if (text[i] == '{') {
            ++depth;
          } else if (text[i] == '}') {
            --depth;
          }
          ++i;
        } while (i < text.size() && depth > 0);
      } else if (text.substr(i, 4) == "null") {
        i += 4;
      } else {
        if (error_out) {
          *error_out = "Invalid scene.json (unknown value): " + scene_path;
        }
        return false;
      }
    }
    skip_ws(text, i);
    if (match_char(text, i, '}')) {
      break;
    }
    if (!match_char(text, i, ',')) {
      if (error_out) {
        *error_out = "Invalid scene.json (trailing): " + scene_path;
      }
      return false;
    }
  }
  if (!saw_entities) {
    if (error_out) {
      *error_out = "Invalid scene.json (missing entities): " + scene_path;
    }
    return false;
  }
  workspace.replace_scene(std::move(entities), selected_id, pan_x, pan_y, zoom,
                          show_grid);
  // Optional editor-tool state (older scene.json files simply omit these).
  if (grid_size) {
    workspace.set_grid_size(*grid_size);
  }
  if (snap) {
    workspace.set_snap_enabled(*snap);
  }
  if (!selection.empty()) {
    workspace.set_selection(selection, workspace.selected_id());
  }
  return true;
}
}  // namespace scene_io
}  // namespace editor
}  // namespace tombstone
}  // namespace ts
