#pragma once

#include "editor/workspace/Workspace2D.h"

#include <string>

namespace ts {
namespace tombstone {
namespace editor {
namespace scene_io {

// scene.json format version written by save(). 1 = entities only;
// 2 = adds optional per-entity "tilemap" and "sprite" objects.
inline constexpr int kSceneVersion = 2;

// Per-project Editor2D persistence: <project_dir>/scene.json
inline std::string scene_path_for_project(const std::string& project_dir) {
  if (project_dir.empty()) {
    return "scene.json";
  }
  if (project_dir.back() == '/' || project_dir.back() == '\\') {
    return project_dir + "scene.json";
  }
  return project_dir + "/scene.json";
}

// Load entities + view state into workspace. Returns false if missing/invalid.
bool load(Workspace2D& workspace, const std::string& scene_path,
          std::string* error_out = nullptr);

// Write workspace entities + view state. Creates parent dirs as needed.
bool save(const Workspace2D& workspace, const std::string& scene_path,
          std::string* error_out = nullptr);

}  // namespace scene_io
}  // namespace editor
}  // namespace tombstone
}  // namespace ts
