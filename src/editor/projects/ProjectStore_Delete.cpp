#include "editor/projects/ProjectStore.h"

#include <filesystem>
#include <system_error>
#include <string>

namespace ts {
namespace tombstone {
namespace editor {

namespace fs = std::filesystem;

bool ProjectStore::delete_project(const std::string& project_path) {
  last_error_.clear();
  if (project_path.empty()) {
    last_error_ = "Cannot delete: empty project path.";
    return false;
  }
  if (projects_root_.empty()) {
    last_error_ = "Cannot delete: projects_root is empty.";
    return false;
  }

  std::error_code ec;
  fs::path root = fs::absolute(fs::path(projects_root_));
  fs::path target = fs::absolute(fs::path(project_path));
  if (fs::exists(root)) {
    root = fs::weakly_canonical(root, ec);
  }
  if (fs::exists(target)) {
    target = fs::weakly_canonical(target, ec);
  }

  const fs::path rel = fs::relative(target, root, ec);
  if (ec || rel.empty() || rel == "." ||
      (!rel.begin()->empty() && *rel.begin() == "..")) {
    last_error_ =
        "Refusing to delete path outside projects_root: " + project_path;
    return false;
  }

  if (!fs::exists(target) || !fs::is_directory(target)) {
    last_error_ = "Project folder not found: " + project_path;
    return false;
  }

  fs::remove_all(target, ec);
  if (ec) {
    last_error_ = "Failed to delete project folder: " + ec.message();
    return false;
  }

  return refresh();
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
