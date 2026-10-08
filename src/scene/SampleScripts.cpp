#include "scene/SampleScripts.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace ts {
namespace tombstone {
namespace sample_scripts {

namespace fs = std::filesystem;

namespace {

constexpr const char* kBlankSource = R"lua(-- A TombStone script. Runs on the server's 60 Hz tick, one copy per
-- entity. `self` is this entity; `props` holds the knobs the Inspector
-- shows (defaults here, per-entity overrides in the scene).
props = {
  speed = 60,
}

-- First tick this entity is in the world.
function on_start()
  log(self:name() .. " is saddled up")
end

-- Every tick; dt is 1/60 s.
function on_tick(dt)
end

-- Somebody rode into this entity's trigger collider (and back out).
function on_trigger_enter(other)
end

function on_trigger_exit(other)
end

-- A rider pressed the action button (E / Space / pad A) close by.
function on_interact(rider)
end
)lua";

constexpr const char* kGateSource = R"lua(-- Corral gate: ride up close and press the action button (E / Space /
-- pad A). The gate swings open (no more wall) and, when close_after is
-- above zero, shuts itself again that many seconds later.
props = {
  open_line = "The gate creaks open.",
  close_after = 0,
  locked = false,
}

function on_start()
  self:set("open", false)
end

local function shut()
  self:set("open", false)
  self:set_collider(true)
  self:show()
  log(self:name() .. " swung shut")
end

function on_interact(rider)
  if self:get("open", false) then
    return
  end
  if props.locked then
    rider:toast("Locked tight. Somebody's got the key.", 2)
    return
  end
  self:set("open", true)
  self:set_collider(false)
  self:hide()
  log(self:name() .. " opened by P" .. rider:slot())
  rider:toast(props.open_line, 2)
  if props.close_after > 0 then
    after(props.close_after, shut)
  end
end
)lua";

constexpr const char* kGoldSource = R"lua(-- Gold nugget: the first rider through its trigger pockets it. The rider
-- hears about it on the HUD, keeps a running "gold" tally, and the
-- nugget leaves the world.
props = {
  amount = 10,
}

function on_trigger_enter(other)
  if not other:is_player() then
    return
  end
  local purse = other:get("gold", 0) + props.amount
  other:set("gold", purse)
  other:toast("Picked up " .. props.amount .. " gold (" .. purse .. " in the purse)", 2.5)
  log("P" .. other:slot() .. " picked up " .. props.amount .. " gold at " .. self:name())
  self:destroy()
end
)lua";

constexpr const char* kSignSource = R"lua(-- Sign post: press the action button nearby to read it.
props = {
  line = "Tombstone, 3 miles. Mind the rattlers.",
  seconds = 3,
}

function on_interact(rider)
  rider:toast(props.line, props.seconds)
end
)lua";

constexpr const char* kTickerSource = R"lua(-- Ticker: does something every few seconds (here: a tumbleweed hop).
props = {
  every = 2,
  hop = 8,
}

local up = true

function on_start()
  every(props.every, function()
    self:move_by(0, up and -props.hop or props.hop)
    up = not up
  end)
end
)lua";

bool write_if_missing(const fs::path& path, const char* text,
                      std::string* error_out) {
  std::error_code ec;
  if (fs::exists(path, ec)) {
    return true;
  }
  fs::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << text;
  if (!out) {
    if (error_out) *error_out = "Could not write " + path.string();
    return false;
  }
  return true;
}

}  // namespace

const std::vector<Template>& templates() {
  static const std::vector<Template> kTemplates = {
      {"blank", "Blank", "Every hook stubbed out, ready to fill in",
       kBlankSource},
      {"gate", "Gate", "Opens on the action button; can shut itself",
       kGateSource},
      {"pickup", "Pickup", "Pocketed by the first rider through its trigger",
       kGoldSource},
      {"sign", "Sign", "Shows a HUD line on the action button", kSignSource},
      {"ticker", "Ticker", "Repeats on a timer (a tumbleweed hop)",
       kTickerSource},
  };
  return kTemplates;
}

const Template* find_template(const std::string& id) {
  for (const Template& t : templates()) {
    if (id == t.id) {
      return &t;
    }
  }
  return nullptr;
}

bool write_into_project(const std::string& project_dir,
                        std::string* error_out) {
  if (project_dir.empty()) {
    if (error_out) *error_out = "no project folder for the sample scripts";
    return false;
  }
  const fs::path dir(project_dir);
  return write_if_missing(dir / kGate, kGateSource, error_out) &&
         write_if_missing(dir / kGold, kGoldSource, error_out);
}

}  // namespace sample_scripts
}  // namespace tombstone
}  // namespace ts
