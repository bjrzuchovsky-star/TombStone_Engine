#include "editor/ui/FolderBrowser.h"

#include <imgui.h>

#include <algorithm>
#include <system_error>

namespace ts {
namespace tombstone {
namespace editor {

namespace fs = std::filesystem;

void FolderBrowser::open(const std::string& start_path) {
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
  selected_dir_ = -1;
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
    ImGui::TextUnformatted("Select a folder for projects_root");
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
    if (dirs_.empty() && error_.empty()) {
      ImGui::TextDisabled("(no subfolders)");
    }
    for (int i = 0; i < static_cast<int>(dirs_.size()); ++i) {
      const bool selected = (selected_dir_ == i);
      if (ImGui::Selectable(dirs_[i].c_str(), selected,
                            ImGuiSelectableFlags_AllowDoubleClick)) {
        selected_dir_ = i;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          navigate_to(current_ / dirs_[static_cast<std::size_t>(i)]);
        }
      }
    }
    ImGui::EndChild();

    if (ImGui::Button("Select This Folder", ImVec2(160, 0))) {
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
