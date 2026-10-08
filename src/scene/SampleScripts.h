#pragma once

// Starter gameplay scripts: the sample corral gate and gold nugget that a
// new scene is seeded with, plus the templates the editor's Scripts panel
// writes new files from. Plain Lua text, ours to ship.

#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace sample_scripts {

inline constexpr const char* kGate = "scripts/gate.lua";
inline constexpr const char* kGold = "scripts/gold.lua";
inline constexpr const char* kGateName = "Corral Gate";
inline constexpr const char* kGoldName = "Gold Nugget";

struct Template {
  const char* id;     // "blank", "gate", ...
  const char* label;  // shown in the editor
  const char* blurb;  // one line: what it does
  const char* source;
};

// Blank (every hook stubbed), Gate, Pickup, Sign, Ticker.
const std::vector<Template>& templates();
// nullptr for an unknown id.
const Template* find_template(const std::string& id);

// Writes kGate and kGold under project_dir unless they already exist (a
// project's own edits are never overwritten). True when both are present.
bool write_into_project(const std::string& project_dir,
                        std::string* error_out = nullptr);

}  // namespace sample_scripts
}  // namespace tombstone
}  // namespace ts
