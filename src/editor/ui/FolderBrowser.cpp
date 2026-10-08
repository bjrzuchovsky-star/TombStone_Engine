#include "editor/ui/FolderBrowser.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <system_error>

namespace ts {
namespace tombstone {
namespace editor {

namespace fs = std::filesystem;

void FolderBrowser::open_file(const std::string& start_path,
                              std::vector<std::string> extensions,
                              std::string prompt) {
  extensions_ = std::move(extensions);
  prompt_ = std::move(prompt);
  file_mode_ = true;
  open_ = true;
  request_open_popup_ = true;
  result_path_.clear();
  error_.clear();
  selected_dir_ = -1;
  selected_file_ = -1;
  std::error_code ec;
  fs::path start = start_path.empty() ? fs::current_path(ec)
                                      : fs::absolute(start_path, ec);
  if (ec || start.empty()) {
    start = fs::path(start_path.empty() ? "." : start_path);
  }
  if (!fs::is_directory(start, ec)) {
    start = start.parent_path();
  }
  if (start.empty()) {
    start = fs::current_path(ec);
  }
  navigate_to(start);
}

void FolderBrowser::open(const std::string& start_path) {
  file_mode_ = false;
  extensions_.clear();
  prompt_.clear();
  files_.clear();
  selected_file_ = -1;
  open_ = true;
  request_open_popup_ = true;
  result_path_.clear();
  error_.clear();
  selected_dir_ = -1;

  fs::path start;
  if (!start_path.empty()) {
    std::error_code ec;
    start = fs::absolute(start_path, ec);
    if (ec) {
      start = fs::path(start_path);
    }
  } else {
    std::error_code ec;
    start = fs::current_path(ec);
    if (ec) {
      start = fs::path(".");
    }
  }

  if (!fs::is_directory(start)) {
    start = start.parent_path();
  }
  if (start.empty()) {
    std::error_code ec;
    start = fs::current_path(ec);
  }
  navigate_to(start);
}

void FolderBrowser::close() {
  open_ = false;
  request_open_popup_ = false;
  selected_dir_ = -1;
}

std::string FolderBrowser::take_result() {
  std::string out = std::move(result_path_);
  result_path_.clear();
  return out;
}

void FolderBrowser::navigate_to(const fs::path& path) {
  std::error_code ec;
  fs::path canonical = fs::weakly_canonical(path, ec);
  if (ec || canonical.empty()) {
    canonical = path;
  }
  current_ = canonical;
  current_display_ = current_.generic_string();
  if (current_display_.empty()) {
    current_display_ = ".";
  }
  refresh_listing();
}

void FolderBrowser::navigate_up() {
  if (!current_.has_parent_path() || current_ == current_.root_path()) {
    return;
  }
  navigate_to(current_.parent_path());
}

void FolderBrowser::refresh_listing() {
  dirs_.clear();
  files_.clear();
  selected_dir_ = -1;
  selected_file_ = -1;
  error_.clear();

  std::error_code ec;
  if (!fs::exists(current_, ec) || !fs::is_directory(current_, ec)) {
    error_ = "Not a readable directory: " + current_display_;
    return;
  }

  for (fs::directory_iterator it(current_, ec), end; it != end && !ec;
       it.increment(ec)) {
    const fs::directory_entry& entry = *it;
    std::error_code entry_ec;
    if (!entry.is_directory(entry_ec)) {
      if (file_mode_ && entry.is_regular_file(entry_ec)) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) {
                         return static_cast<char>(std::tolower(c));
                       });
        if (extensions_.empty() ||
            std::find(extensions_.begin(), extensions_.end(), ext) !=
                extensions_.end()) {
          files_.push_back(entry.path().filename().generic_string());
        }
      }
      continue;
    }
    dirs_.push_back(entry.path().filename().generic_string());
  }
  if (ec) {
    error_ = "Failed to list directory: " + ec.message();
  }
  std::sort(dirs_.begin(), dirs_.end(),
            [](const std::string& a, const std::string& b) {
              return a < b;
            });
  std::sort(files_.begin(), files_.end());
}

bool FolderBrowser::draw(const char* popup_id) {
  if (!open_) {
    return false;
  }

  if (request_open_popup_) {
    ImGui::OpenPopup(popup_id);
    request_open_popup_ = false;
  }

  bool confirmed = false;
  const ImGuiViewport* vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(
      ImVec2(vp->GetCenter().x, vp->GetCenter().y), ImGuiCond_Appearing,
      ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_Appearing);

  if (ImGui::BeginPopupModal(popup_id, nullptr,
                             ImGuiWindowFlags_NoResize)) {
    ImGui::TextUnformatted(file_mode_ ? prompt_.c_str()
                                      : "Select a folder for projects_root");
    ImGui::Separator();

    ImGui::TextWrapped("%s", current_display_.c_str());
    ImGui::Spacing();

    if (ImGui::Button("Up##folder_up", ImVec2(80, 0))) {
      navigate_up();
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh##folder_refresh", ImVec2(80, 0))) {
      refresh_listing();
    }
    ImGui::SameLine();
    if (ImGui::Button("Home (cwd)##folder_cwd", ImVec2(120, 0))) {
      std::error_code ec;
      navigate_to(fs::current_path(ec));
    }

    if (!error_.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.35f, 1.0f), "%s",
                         error_.c_str());
    }

    ImGui::BeginChild("##FolderList", ImVec2(0, -48),
                      ImGuiChildFlags_Borders);
    if (dirs_.empty() && files_.empty() && error_.empty()) {
      ImGui::TextDisabled(file_mode_ ? "(nothing here)" : "(no subfolders)");
    }
    bool navigated = false;
    for (int i = 0; i < static_cast<int>(dirs_.size()) && !navigated; ++i) {
      const bool selected = (selected_dir_ == i);
      const std::string label =
          file_mode_ ? "[dir] " + dirs_[static_cast<std::size_t>(i)]
                     : dirs_[static_cast<std::size_t>(i)];
      if (ImGui::Selectable(label.c_str(), selected,
                            ImGuiSelectableFlags_AllowDoubleClick)) {
        selected_dir_ = i;
        selected_file_ = -1;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          navigate_to(current_ / dirs_[static_cast<std::size_t>(i)]);
          navigated = true;
        }
      }
    }
    for (int i = 0; i < static_cast<int>(files_.size()) && !navigated; ++i) {
      const bool selected = (selected_file_ == i);
      if (ImGui::Selectable(files_[static_cast<std::size_t>(i)].c_str(),
                            selected, ImGuiSelectableFlags_AllowDoubleClick)) {
        selected_file_ = i;
        selected_dir_ = -1;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          result_path_ =
              (current_ / files_[static_cast<std::size_t>(i)]).string();
          confirmed = true;
          open_ = false;
          ImGui::CloseCurrentPopup();
        }
      }
    }
    ImGui::EndChild();

    if (file_mode_) {
      const bool has_file =
          selected_file_ >= 0 &&
          selected_file_ < static_cast<int>(files_.size());
      ImGui::BeginDisabled(!has_file || confirmed);
      if (ImGui::Button("Pick File", ImVec2(160, 0)) && has_file) {
        result_path_ =
            (current_ / files_[static_cast<std::size_t>(selected_file_)])
                .string();
        confirmed = true;
        open_ = false;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndDisabled();
    } else if (ImGui::Button("Select This Folder", ImVec2(160, 0))) {
      result_path_ = current_display_;
      confirmed = true;
      open_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (selected_dir_ >= 0 &&
        selected_dir_ < static_cast<int>(dirs_.size())) {
      if (ImGui::Button("Open Selected", ImVec2(120, 0))) {
        navigate_to(current_ /
                    dirs_[static_cast<std::size_t>(selected_dir_)]);
      }
      ImGui::SameLine();
    }
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
      open_ = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  } else if (open_) {
    // Popup closed externally (Esc) — treat as cancel.
    open_ = false;
  }

  return confirmed;
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
