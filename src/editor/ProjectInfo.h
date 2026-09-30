#pragma once

#include <string>

namespace ts {
namespace tombstone {
namespace editor {

enum class ProjectKind {
  TwoD,
  ThreeD,
};

// On-disk project descriptor (folder under projects_root + project.json).
struct ProjectInfo {
  std::string id;              // folder name under projects_root
  std::string name;            // display name from project.json
  ProjectKind kind = ProjectKind::TwoD;
  std::string path;            // absolute or cwd-relative project folder
  std::string created;         // ISO-ish timestamp string
  std::string last_opened;     // optional; empty if never opened
};

inline const char* to_string(ProjectKind kind) {
  switch (kind) {
    case ProjectKind::TwoD:
      return "2D";
    case ProjectKind::ThreeD:
      return "3D";
  }
  return "Unknown";
}

inline const char* dimension_string(ProjectKind kind) {
  switch (kind) {
    case ProjectKind::TwoD:
      return "2d";
    case ProjectKind::ThreeD:
      return "3d";
  }
  return "2d";
}

inline ProjectKind project_kind_from_dimension(const std::string& dimension) {
  if (dimension == "3d" || dimension == "3D") {
    return ProjectKind::ThreeD;
  }
  return ProjectKind::TwoD;
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
