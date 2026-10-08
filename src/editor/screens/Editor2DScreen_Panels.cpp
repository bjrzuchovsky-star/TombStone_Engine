#include "editor/screens/Editor2DScreen.h"

#include "editor/ui/Theme.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <optional>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

void Editor2DScreen::draw_hierarchy() {
  hierarchy_focused_ =
      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

  char caption[64];
  std::snprintf(caption, sizeof(caption), "%zu entit%s | %zu selected",
                workspace_.entities().size(),
                workspace_.entities().size() == 1 ? "y" : "ies",
                workspace_.selection_count());
  theme::SectionHeader("Scene", caption);

  const float bw = std::max(54.0f, (ImGui::GetContentRegionAvail().x -
                                    3.0f * ImGui::GetStyle().ItemSpacing.x) /
                                       4.0f);
  if (theme::PrimaryButton("+ New", ImVec2(bw, 0))) {
    cancel_rename();
    workspace_.create_entity("Entity");
    mark_dirty_and_autosave();
  }
  const bool has_sel = workspace_.selection_count() > 0;
  ImGui::BeginDisabled(!has_sel);
  ImGui::SameLine();
  if (theme::CopperButton("Dupe", ImVec2(bw, 0))) {
    duplicate_selected();
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("Duplicate selection (Ctrl+D)");
  }
  ImGui::SameLine();
  if (theme::SecondaryButton("Rename", ImVec2(bw, 0))) {
    if (const Entity2D* e = workspace_.selected()) {
      begin_rename(e->id);
    }
  }
  ImGui::SameLine();
  if (theme::DangerButton("Delete", ImVec2(bw, 0))) {
    delete_selected();
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("Delete selection (Del)");
  }
  ImGui::EndDisabled();

  theme::Divider();

  ImGui::SetNextItemOpen(true, ImGuiCond_Once);
  if (ImGui::TreeNodeEx("Scene##root",
                        ImGuiTreeNodeFlags_DefaultOpen |
                            ImGuiTreeNodeFlags_SpanAvailWidth)) {
    const ImGuiIO& io = ImGui::GetIO();
    // Deferred so the list is not mutated mid-iteration.
    std::optional<std::uint64_t> delete_one;
    bool delete_sel = false;
    bool dupe_sel = false;

    const auto& ents = workspace_.entities();
    for (std::size_t i = 0; i < ents.size(); ++i) {
      const Entity2D& e = ents[i];
      const bool selected = workspace_.is_selected(e.id);
      const bool primary = workspace_.selected_id().has_value() &&
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
      if (primary) {
        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(
            mn, ImVec2(mn.x + 3.0f, mx.y), theme::U32(theme::Accent()));
      }
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        const bool ctrl = io.KeyCtrl || io.KeySuper;
        if (io.KeyShift && has_range_anchor_ && range_anchor_ < ents.size()) {
          // Shift+click: range from the anchor row (Ctrl+Shift extends).
          const std::size_t lo = std::min(range_anchor_, i);
          const std::size_t hi = std::max(range_anchor_, i);
          std::vector<std::uint64_t> ids;
          if (ctrl) {
            ids = workspace_.selection();
          }
          for (std::size_t k = lo; k <= hi; ++k) {
            ids.push_back(ents[k].id);
          }
          workspace_.set_selection(ids, e.id);
        } else if (ctrl) {
          workspace_.toggle_selection(e.id);
          range_anchor_ = i;
          has_range_anchor_ = true;
        } else {
          workspace_.select(e.id);
          range_anchor_ = i;
          has_range_anchor_ = true;
        }
      }
      if (ImGui::IsItemHovered() &&
          ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        begin_rename(e.id);
      }
      if (ImGui::BeginPopupContextItem()) {
        if (!selected) {
          workspace_.select(e.id);
        }
        const std::size_t n = workspace_.selection_count();
        if (ImGui::MenuItem("Rename", "F2", false, n == 1)) {
          begin_rename(e.id);
        }
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
          dupe_sel = true;
        }
        if (n > 1) {
          if (ImGui::MenuItem("Delete selection", "Del")) {
            delete_sel = true;
          }
        } else if (ImGui::MenuItem("Delete", "Del")) {
          delete_one = e.id;
        }
        ImGui::EndPopup();
      }
    }
    ImGui::TreePop();

    if (dupe_sel) {
      duplicate_selected();
    } else if (delete_sel) {
      delete_selected();
    } else if (delete_one) {
      cancel_rename();
      if (workspace_.delete_entity(*delete_one)) {
        has_range_anchor_ = false;
        mark_dirty_and_autosave();
        note("Buried 1 entity");
      }
    }
  }

  // Click on empty space below the list clears the selection.
  const ImVec2 rest = ImGui::GetContentRegionAvail();
  if (rest.x > 1.0f && rest.y > 1.0f) {
    ImGui::InvisibleButton("##HierarchyEmpty", rest);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) &&
        !(ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift)) {
      workspace_.clear_selection();
    }
  }
}

bool Editor2DScreen::transform_field(const char* label, float* field, int slot,
                                     float vmin, float vmax, bool is_extent) {
  const bool snap = workspace_.snap_enabled();
  const std::uint64_t owner =
      workspace_.selected_id() ? *workspace_.selected_id() : 0;
  const bool tracking = insp_drag_slot_ == slot && insp_drag_entity_ == owner;
  // While dragging with snap on, edit an unsnapped shadow value so small
  // mouse motions accumulate instead of being rounded away every frame.
  float v = (snap && tracking) ? insp_drag_raw_ : *field;
  const float speed = snap ? std::max(0.5f, workspace_.grid_size() * 0.05f)
                           : 0.5f;
  const bool changed =
      ImGui::DragFloat(label, &v, speed, vmin, vmax, snap ? "%.0f" : "%.2f");
  if (ImGui::IsItemActive()) {
    insp_drag_slot_ = slot;
    insp_drag_entity_ = owner;
    insp_drag_raw_ = v;
  } else if (tracking) {
    insp_drag_slot_ = -1;
  }
  bool applied = false;
  if (changed) {
    float nv = v;
    if (snap) {
      nv = is_extent ? workspace_.snap_extent(v) : workspace_.snap_value(v);
    }
    nv = std::clamp(nv, vmin, vmax);
    if (nv != *field) {
      *field = nv;
      applied = true;
      mark_dirty();
    }
  }
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    if (snap) {
      *field = is_extent ? workspace_.snap_extent(*field)
                         : workspace_.snap_value(*field);
    }
    mark_dirty_and_autosave();
  }
  return applied;
}

void Editor2DScreen::draw_inspector() {
  Entity2D* e = workspace_.selected();
  const std::size_t count = workspace_.selection_count();
  if (!e) {
    theme::SectionHeader("Inspector", "nothing selected");
    ImGui::Spacing();
    theme::StatusInfo(
        "Nothing in the crosshairs. Click an entity in the viewport or the "
        "Scene list. Ctrl+click or drag a box to round up several.");
  } else {
    char sel_caption[48];
    std::snprintf(sel_caption, sizeof(sel_caption), "%zu selected", count);
    theme::SectionHeader("Inspector", sel_caption);
    if (count > 1) {
      char buf[96];
      std::snprintf(buf, sizeof(buf),
                    "%zu selected. Fields edit the primary (amber); drag, "
                    "nudge, Dupe and Delete hit them all.",
                    count);
      theme::StatusWarn(buf);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, theme::Accent());
    ImGui::TextUnformatted(e->name.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("id %llu", static_cast<unsigned long long>(e->id));
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

    ImGui::SeparatorText(workspace_.snap_enabled() ? "Transform  [snap]"
                                                   : "Transform");
    transform_field("X", &e->x, 0, -1.0e6f, 1.0e6f, false);
    transform_field("Y", &e->y, 1, -1.0e6f, 1.0e6f, false);
    transform_field("W", &e->w, 2, 1.0f, 4096.0f, true);
    transform_field("H", &e->h, 3, 1.0f, 4096.0f, true);
    if (count > 1 || !workspace_.snap_enabled()) {
      if (theme::SecondaryButton("Snap selection to grid", ImVec2(-1, 0))) {
        if (workspace_.snap_selection_to_grid()) {
          mark_dirty_and_autosave();
          note("Snapped to grid");
        }
      }
    }

    ImGui::SeparatorText("Appearance");
    if (ImGui::ColorEdit4("Tint", e->color,
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
  theme::Divider();
  if (theme::SecondaryButton("Back to Projects", ImVec2(-1, 0))) {
    request_back_to_projects();
  }
}

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
