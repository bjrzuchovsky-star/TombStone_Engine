#pragma once

#include <string>

namespace ts {
namespace tombstone {
namespace editor {

enum class ProjectKind {
  TwoD,
  ThreeD,
};

struct ProjectInfo {
  std::string id;
  std::string name;
  ProjectKind kind = ProjectKind::TwoD;
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

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
