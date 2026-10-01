#include "editor/screens/Editor2DScreen.h"

#include "editor/ui/Theme.h"

#include <imgui.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

void Editor2DScreen::draw_hierarchy() {
  theme::SectionHeader("Hierarchy");
  ImGui::TextDisabled("%zu entit%s", workspace_.entities().size(),
                      workspace_.entities().size() == 1 ? "y" : "ies");
  ImGui::Separator();

  if (theme::PrimaryButton("Create", ImVec2(70, 0))) {
    cancel_rename();
    workspace_.create_entity("Entity");
    mark_dirty_and_autosave();
  }
  ImGui::SameLine();
  const bool has_sel = workspace_.selected() != nullptr;
  if (!has_sel) {
    ImGui::BeginDisabled();
  }
  if (theme::SecondaryButton("Rename", ImVec2(70, 0))) {
    if (const Entity2D* e = workspace_.selected()) {
      begin_rename(e->id);
    }
  }
  ImGui::SameLine();
  if (theme::DangerButton("Delete", ImVec2(70, 0))) {
    if (const Entity2D* e = workspace_.selected()) {
      const std::uint64_t id = e->id;
      cancel_rename();
      if (workspace_.delete_entity(id)) {
        mark_dirty_and_autosave();
      }
    }
  }
  if (!has_sel) {
    ImGui::EndDisabled();
  }

  ImGui::Separator();

  ImGui::SetNextItemOpen(true, ImGuiCond_Once);
  if (ImGui::TreeNodeEx("Scene##root",
                        ImGuiTreeNodeFlags_DefaultOpen |
                            ImGuiTreeNodeFlags_SpanAvailWidth)) {
    for (const Entity2D& e : workspace_.entities()) {
      const bool selected =
          workspace_.selected_id().has_value() &&
          *workspace_.selected_id() == e.id;

      if (renaming_ && rename_id_ == e.id) {
        ImGui::PushID(static_cast<int>(e.id));
        ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##rename", rename_buf_, sizeof(rename_buf_),
                             ImGuiInputTextFlags_EnterReturnsTrue |
                                 ImGuiInputTextFlags_AutoSelectAll)) {
          commit_rename();
        }
        if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0) &&
            !ImGui::IsItemHovered()) {
          commit_rename();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
          cancel_rename();
        }
        ImGui::PopID();
        continue;
      }

      ImGuiTreeNodeFlags flags =
          ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
          ImGuiTreeNodeFlags_SpanAvailWidth;
      if (selected) {
        flags |= ImGuiTreeNodeFlags_Selected;
      }
      ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(e.id)),
                        flags, "%s", e.name.c_str());
      if (ImGui::IsItemClicked()) {
        workspace_.select(e.id);
      }
      if (ImGui::IsItemHovered() &&
          ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        begin_rename(e.id);
      }
      if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Rename")) {
          begin_rename(e.id);
        }
        if (ImGui::MenuItem("Delete")) {
          if (workspace_.delete_entity(e.id)) {
            mark_dirty_and_autosave();
          }
        }
        ImGui::EndPopup();
      }
    }
    ImGui::TreePop();
  }
}

void Editor2DScreen::draw_inspector() {
  theme::SectionHeader("Inspector");
  ImGui::Separator();

  Entity2D* e = workspace_.selected();
  if (!e) {
    ImGui::TextDisabled("Selection: (none)");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Select an entity in Hierarchy or click it in the viewport.");
  } else {
    ImGui::Text("Selection: %s  (id %llu)", e->name.c_str(),
                static_cast<unsigned long long>(e->id));
    ImGui::Spacing();

    char name_buf[128];
    std::memset(name_buf, 0, sizeof(name_buf));
    std::strncpy(name_buf, e->name.c_str(), sizeof(name_buf) - 1);
    if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
      if (name_buf[0] != '\0') {
        e->name = name_buf;
        mark_dirty();
      }
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      mark_dirty_and_autosave();
    }

    ImGui::SeparatorText("Transform");
    ImGui::DragFloat("X", &e->x, 0.5f);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      mark_dirty_and_autosave();
    }
    ImGui::DragFloat("Y", &e->y, 0.5f);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      mark_dirty_and_autosave();
    }
    ImGui::DragFloat("W", &e->w, 0.5f, 1.0f, 4096.0f);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      mark_dirty_and_autosave();
    }
    ImGui::DragFloat("H", &e->h, 0.5f, 1.0f, 4096.0f);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      mark_dirty_and_autosave();
    }

    ImGui::SeparatorText("Appearance");
    if (ImGui::ColorEdit4("Color / Tint", e->color,
                          ImGuiColorEditFlags_AlphaBar |
                              ImGuiColorEditFlags_Float)) {
      mark_dirty();
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      mark_dirty_and_autosave();
    }
    if (ImGui::InputInt("Layer / Z", &e->layer)) {
      mark_dirty();
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      mark_dirty_and_autosave();
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  if (theme::SecondaryButton("Back to Projects", ImVec2(-1, 0))) {
    request_back_to_projects();
  }
}


}  // namespace editor
}  // namespace tombstone
}  // namespace ts
