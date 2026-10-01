#include "editor/AppFlow.h"

#include "editor/screens/ProjectManagerScreen.h"

#include <iostream>

namespace ts {
namespace tombstone {
namespace editor {

void AppFlow::handle_pending_screen_actions() {
  if (auto* pm = dynamic_cast<ProjectManagerScreen*>(screen_.get())) {
    if (pm->new_project_requested()) {
      const std::string name = pm->pending_new_project_name();
      pm->clear_new_project_request();
      create_new_project_2d(name);
    }
    if (pm->delete_requested()) {
      const auto idx = pm->pending_delete_index();
      pm->clear_delete_request();
      if (idx.has_value()) {
        delete_project(*idx);
      }
    }
  }
}

bool AppFlow::delete_project(std::size_t index) {
  last_error_.clear();
  if (index >= project_store_.projects().size()) {
    last_error_ = "Invalid project index for delete.";
    if (auto* pm = dynamic_cast<ProjectManagerScreen*>(screen_.get())) {
      pm->set_error_message(last_error_);
    }
    return false;
  }
  const ProjectInfo victim = project_store_.projects()[index];
  if (!project_store_.delete_project(victim.path)) {
    last_error_ = project_store_.last_error();
    status_message_.clear();
    std::cout << "[AppFlow] delete project failed: " << last_error_ << '\n';
    if (auto* pm = dynamic_cast<ProjectManagerScreen*>(screen_.get())) {
      pm->set_error_message(last_error_);
      pm->set_status_message({});
    }
    return false;
  }
  status_message_ = "Deleted project \"" + victim.name + "\"";
  std::cout << "[AppFlow] " << status_message_ << '\n';
  if (auto* pm = dynamic_cast<ProjectManagerScreen*>(screen_.get())) {
    pm->set_projects(project_store_.projects());
    pm->set_status_message(status_message_);
    pm->set_error_message({});
  }
  return true;
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
