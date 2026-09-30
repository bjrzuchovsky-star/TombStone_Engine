#pragma once

#include "editor/ProjectInfo.h"

#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

// Scans projects_root for project folders (each containing project.json).
// Creates new 2D projects on disk and seeds a sample when the root is empty.
class ProjectStore {
 public:
  explicit ProjectStore(std::string projects_root);

  const std::string& projects_root() const { return projects_root_; }
  void set_projects_root(std::string projects_root);

  const std::vector<ProjectInfo>& projects() const { return projects_; }
  const std::string& last_error() const { return last_error_; }

  // Ensures root exists, scans for project.json folders, seeds sample if empty.
  // Returns false on hard failures (missing/unwritable root).
  bool refresh();

  // Creates <root>/<slug>/project.json for a new 2D project. Refreshes list.
  // Returns false if name empty, path conflict, or I/O error.
  bool create_project_2d(const std::string& name, ProjectInfo* out = nullptr);

  // Marks last_opened on disk and in the in-memory list.
  bool touch_last_opened(const std::string& project_path);

  // Load a single project.json from a folder (no list mutation).
  static bool load_project_json(const std::string& project_dir,
                                ProjectInfo* out,
                                std::string* error_out = nullptr);

  static bool write_project_json(const ProjectInfo& project,
                                 std::string* error_out = nullptr);

 private:
  static std::string make_slug(const std::string& name);
  static std::string now_timestamp();
  bool seed_sample_2d_if_empty();

  std::string projects_root_;
  std::vector<ProjectInfo> projects_;
  std::string last_error_;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
