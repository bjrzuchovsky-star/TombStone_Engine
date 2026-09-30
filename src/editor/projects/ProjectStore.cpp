#include "editor/projects/ProjectStore.h"

#include "editor/settings/JsonMini.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace ts {
namespace tombstone {
namespace editor {

namespace fs = std::filesystem;

ProjectStore::ProjectStore(std::string projects_root)
    : projects_root_(std::move(projects_root)) {}

void ProjectStore::set_projects_root(std::string projects_root) {
  projects_root_ = std::move(projects_root);
  projects_.clear();
  last_error_.clear();
}

std::string ProjectStore::now_timestamp() {
  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  const std::time_t t = clock::to_time_t(now);
  std::tm tm_buf{};
#if defined(_WIN32)
  gmtime_s(&tm_buf, &t);
#else
  gmtime_r(&t, &tm_buf);
#endif
  std::ostringstream ss;
  ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

std::string ProjectStore::make_slug(const std::string& name) {
  std::string slug;
  slug.reserve(name.size());
  bool pending_dash = false;
  for (unsigned char ch : name) {
    if (std::isalnum(ch)) {
      if (pending_dash && !slug.empty()) {
        slug.push_back('-');
      }
      pending_dash = false;
      slug.push_back(static_cast<char>(std::tolower(ch)));
    } else if (!slug.empty()) {
      pending_dash = true;
    }
  }
  if (slug.empty()) {
    slug = "project";
  }
  return slug;
}

bool ProjectStore::load_project_json(const std::string& project_dir,
                                     ProjectInfo* out,
                                     std::string* error_out) {
  if (!out) {
    if (error_out) {
      *error_out = "load_project_json: out is null";
    }
    return false;
  }

  const fs::path dir = project_dir;
  const fs::path json_path = dir / "project.json";
  if (!fs::exists(json_path)) {
    if (error_out) {
      *error_out = "Missing project.json in " + dir.string();
    }
    return false;
  }

  std::ifstream in(json_path);
  if (!in) {
    if (error_out) {
      *error_out = "Could not open " + json_path.string();
    }
    return false;
  }

  std::ostringstream ss;
  ss << in.rdbuf();
  auto parsed = json_mini::parse_object(ss.str());
  if (!parsed) {
    if (error_out) {
      *error_out = "Invalid project.json: " + json_path.string();
    }
    return false;
  }

  ProjectInfo info;
  info.path = dir.string();
  info.id = dir.filename().string();
  info.name = json_mini::get_string(*parsed, "name", info.id);
  info.kind = project_kind_from_dimension(
      json_mini::get_string(*parsed, "dimension", "2d"));
  info.created = json_mini::get_string(*parsed, "created");
  info.last_opened = json_mini::get_string(*parsed, "last_opened");
  *out = std::move(info);
  return true;
}

bool ProjectStore::write_project_json(const ProjectInfo& project,
                                      std::string* error_out) {
  if (project.path.empty()) {
    if (error_out) {
      *error_out = "Project path is empty";
    }
    return false;
  }

  std::error_code ec;
  fs::create_directories(project.path, ec);
  if (ec) {
    if (error_out) {
      *error_out = "Could not create project folder '" + project.path +
                   "': " + ec.message();
    }
    return false;
  }

  const fs::path json_path = fs::path(project.path) / "project.json";
  std::ofstream out(json_path, std::ios::trunc);
  if (!out) {
    if (error_out) {
      *error_out = "Could not write " + json_path.string();
    }
    return false;
  }

  std::vector<std::pair<std::string, std::string>> fields = {
      {"name", project.name},
      {"dimension", dimension_string(project.kind)},
      {"created", project.created},
  };
  if (!project.last_opened.empty()) {
    fields.emplace_back("last_opened", project.last_opened);
  }

  out << json_mini::write_object(fields);
  if (!out) {
    if (error_out) {
      *error_out = "Failed while writing " + json_path.string();
    }
    return false;
  }
  return true;
}

bool ProjectStore::seed_sample_2d_if_empty() {
  if (!projects_.empty()) {
    return true;
  }

  ProjectInfo sample;
  sample.id = "sample-2d-platformer";
  sample.name = "Sample 2D Platformer";
  sample.kind = ProjectKind::TwoD;
  sample.path = (fs::path(projects_root_) / sample.id).string();
  sample.created = now_timestamp();

  if (!write_project_json(sample, &last_error_)) {
    return false;
  }
  projects_.push_back(std::move(sample));
  return true;
}

bool ProjectStore::refresh() {
  projects_.clear();
  last_error_.clear();

  if (projects_root_.empty()) {
    last_error_ = "projects_root path is empty";
    return false;
  }

  const fs::path root = projects_root_;
  std::error_code ec;
  if (!fs::exists(root)) {
    fs::create_directories(root, ec);
    if (ec) {
      last_error_ = "Could not create projects_root '" + root.string() +
                    "': " + ec.message();
      return false;
    }
  } else if (!fs::is_directory(root)) {
    last_error_ = "projects_root is not a directory: " + root.string();
    return false;
  }

  for (const auto& entry : fs::directory_iterator(root, ec)) {
    if (ec) {
      last_error_ = "Failed scanning projects_root: " + ec.message();
      return false;
    }
    if (!entry.is_directory()) {
      continue;
    }
    ProjectInfo info;
    std::string err;
    if (!load_project_json(entry.path().string(), &info, &err)) {
      // Skip folders without a valid project.json.
      continue;
    }
    projects_.push_back(std::move(info));
  }

  std::sort(projects_.begin(), projects_.end(),
            [](const ProjectInfo& a, const ProjectInfo& b) {
              return a.name < b.name;
            });

  if (!seed_sample_2d_if_empty()) {
    return false;
  }
  return true;
}

bool ProjectStore::create_project_2d(const std::string& name,
                                     ProjectInfo* out) {
  last_error_.clear();
  if (name.empty()) {
    last_error_ = "Project name must be non-empty";
    return false;
  }

  std::error_code ec;
  if (!fs::exists(projects_root_)) {
    fs::create_directories(projects_root_, ec);
    if (ec) {
      last_error_ = "Could not create projects_root: " + ec.message();
      return false;
    }
  }

  std::string slug = make_slug(name);
  fs::path dir = fs::path(projects_root_) / slug;
  int suffix = 2;
  while (fs::exists(dir)) {
    dir = fs::path(projects_root_) / (slug + "-" + std::to_string(suffix));
    ++suffix;
  }

  ProjectInfo info;
  info.id = dir.filename().string();
  info.name = name;
  info.kind = ProjectKind::TwoD;
  info.path = dir.string();
  info.created = now_timestamp();

  if (!write_project_json(info, &last_error_)) {
    return false;
  }

  if (!refresh()) {
    return false;
  }

  if (out) {
    // Re-load from refreshed list if present.
    for (const auto& p : projects_) {
      if (p.path == info.path || p.id == info.id) {
        *out = p;
        return true;
      }
    }
    *out = info;
  }
  return true;
}

bool ProjectStore::touch_last_opened(const std::string& project_path) {
  last_error_.clear();
  ProjectInfo info;
  if (!load_project_json(project_path, &info, &last_error_)) {
    return false;
  }
  info.last_opened = now_timestamp();
  if (!write_project_json(info, &last_error_)) {
    return false;
  }
  for (auto& p : projects_) {
    if (p.path == info.path || p.id == info.id) {
      p.last_opened = info.last_opened;
      break;
    }
  }
  return true;
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
