#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

// Portable ImGui directory browser modal (no extra deps).
// Call open() then draw() each frame while visible; take_result() after OK.
// open_file() switches to a simple file picker: files whose extension is in
// `extensions` (lower-case, with dot) are listed and the result is the
// picked file's path.
class FolderBrowser {
 public:
  void open(const std::string& start_path = {});
  void open_file(const std::string& start_path,
                 std::vector<std::string> extensions,
                 std::string prompt = "Pick a file");
  bool file_mode() const { return file_mode_; }
  void close();
  bool is_open() const { return open_; }

  // Draw modal if open. Returns true when the user confirms a selection.
  bool draw(const char* popup_id = "Select Folder");

  // Non-empty after a successful confirm; cleared by take_result().
  bool has_result() const { return !result_path_.empty(); }
  std::string take_result();

 private:
  void refresh_listing();
  void navigate_to(const std::filesystem::path& path);
  void navigate_up();

  bool open_ = false;
  bool request_open_popup_ = false;
  std::filesystem::path current_;
  std::string current_display_;
  std::vector<std::string> dirs_;
  std::vector<std::string> files_;
  std::vector<std::string> extensions_;
  std::string prompt_;
  bool file_mode_ = false;
  int selected_file_ = -1;
  std::string error_;
  std::string result_path_;
  int selected_dir_ = -1;
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
