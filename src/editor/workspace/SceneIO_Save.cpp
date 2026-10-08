#include "editor/workspace/SceneIO.h"

#include "editor/settings/JsonMini.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace ts {
namespace tombstone {
namespace editor {
namespace scene_io {

namespace fs = std::filesystem;

namespace {

using json_mini::escape_string;

std::string format_number(double v) {
  std::ostringstream ss;
  ss.setf(std::ios::fmtflags(0), std::ios::floatfield);
  ss.precision(9);
  ss << v;
  return ss.str();
}

}  // namespace

bool save(const Workspace2D& workspace, const std::string& scene_path,
          std::string* error_out) {
  if (scene_path.empty()) {
    if (error_out) {
      *error_out = "scene path is empty";
    }
    return false;
  }

  const fs::path path = scene_path;
  std::error_code ec;
  if (path.has_parent_path()) {
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
      if (error_out) {
        *error_out = "Could not create directory for " + scene_path + ": " +
                     ec.message();
      }
      return false;
    }
  }

  std::ofstream out(path, std::ios::trunc);
  if (!out) {
    if (error_out) {
      *error_out = "Could not write " + scene_path;
    }
    return false;
  }

  out << "{\n";
  out << "  \"version\": 1,\n";
  out << "  \"pan_x\": " << format_number(workspace.pan_x()) << ",\n";
  out << "  \"pan_y\": " << format_number(workspace.pan_y()) << ",\n";
  out << "  \"zoom\": " << format_number(workspace.zoom()) << ",\n";
  out << "  \"show_grid\": " << (workspace.show_grid() ? "true" : "false")
      << ",\n";
  out << "  \"grid_size\": " << format_number(workspace.grid_size()) << ",\n";
  out << "  \"snap\": " << (workspace.snap_enabled() ? "true" : "false")
      << ",\n";
  if (workspace.selected_id()) {
    out << "  \"selected_id\": "
        << static_cast<unsigned long long>(*workspace.selected_id()) << ",\n";
  } else {
    out << "  \"selected_id\": null,\n";
  }
  out << "  \"selection\": [";
  for (std::size_t i = 0; i < workspace.selection().size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << static_cast<unsigned long long>(workspace.selection()[i]);
  }
  out << "],\n";
  out << "  \"entities\": [\n";
  const auto& entities = workspace.entities();
  for (std::size_t idx = 0; idx < entities.size(); ++idx) {
    const Entity2D& e = entities[idx];
    out << "    {\n";
    out << "      \"id\": " << static_cast<unsigned long long>(e.id) << ",\n";
    out << "      \"name\": \"" << escape_string(e.name) << "\",\n";
    out << "      \"x\": " << format_number(e.x) << ",\n";
    out << "      \"y\": " << format_number(e.y) << ",\n";
    out << "      \"w\": " << format_number(e.w) << ",\n";
    out << "      \"h\": " << format_number(e.h) << ",\n";
    out << "      \"r\": " << format_number(e.color[0]) << ",\n";
    out << "      \"g\": " << format_number(e.color[1]) << ",\n";
    out << "      \"b\": " << format_number(e.color[2]) << ",\n";
    out << "      \"a\": " << format_number(e.color[3]) << ",\n";
    out << "      \"layer\": " << e.layer << "\n";
    out << "    }";
    if (idx + 1 < entities.size()) {
      out << ",";
    }
    out << "\n";
  }
  out << "  ]\n";
  out << "}\n";

  if (!out) {
    if (error_out) {
      *error_out = "Failed while writing " + scene_path;
    }
    return false;
  }
  return true;
}

}  // namespace scene_io
}  // namespace editor
}  // namespace tombstone
}  // namespace ts
