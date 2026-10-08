// Headless --smoke for the editor's scripting tools: the seeded gate and
// gold nugget, script props in the Inspector (undo + autosave), the
// Scripts panel (templates), Play with scripts (Telegraph lines, toasts, a
// quick action tap, a runtime error benching one instance, hot reload, a
// broken save) and Stop putting back everything the scripts changed.

#include "SmokeScripting.h"

#include "editor/ProjectInfo.h"
#include "editor/console/TelegraphLog.h"
#include "editor/screens/Editor2DScreen.h"
#include "editor/workspace/SceneIO.h"
#include "runtime/Log.h"
#include "runtime/PlaySession.h"
#include "runtime/World.h"
#include "scene/SampleScripts.h"
#include "scene/SceneData.h"
#include "scene/SceneJson.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>

namespace {

using namespace ts::tombstone;
namespace fs = std::filesystem;

int fail(const std::string& what) {
  std::cerr << "[smoke] script editor FAILED: " << what << '\n';
  return 1;
}

std::string read_text(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void write_text(const fs::path& p, const std::string& text) {
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  out << text;
}

bool has(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

std::string doc_text(const editor::Workspace2D& w) {
  return scene_json::write(editor::scene_io::to_doc(w));
}

runtime::InputFrame ride(float x, std::uint32_t buttons = 0) {
  runtime::InputFrame f;
  f.slot(0).move_x = x;
  f.slot(0).buttons = buttons;
  f.slot(0).connected = true;
  return f;
}

// First Telegraph line matching `pred`, or nullptr.
const editor::TelegraphLog::Line* find_line(
    const editor::TelegraphLog& log,
    const std::function<bool(const runtime::LogEntry&)>& pred) {
  for (const editor::TelegraphLog::Line& l : log.lines()) {
    if (pred(l.entry)) return &l;
  }
  return nullptr;
}

bool wire_has(const editor::TelegraphLog& log, const std::string& needle) {
  return find_line(log, [&](const runtime::LogEntry& e) {
           return has(runtime::format_log(e, true), needle);
         }) != nullptr;
}

}  // namespace

int run_script_editor_smoke() {
  const fs::path dir = fs::path("TombStoneProjects") / "_script_smoke";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  editor::ProjectInfo info;
  info.id = "_script_smoke";
  info.name = "Script Smoke";
  info.path = dir.string();
  editor::Editor2DScreen scr(info);
  scr.on_enter();
  editor::Workspace2D& w = scr.workspace();
  editor::TelegraphLog& wire = scr.telegraph();
  const fs::path scene = editor::scene_io::scene_path_for_project(info.path);

  // A fresh scene rides in with the sample gate and nugget and their files.
  const Entity2D* gate_e = find_entity_named(w.entities(), sample_scripts::kGateName);
  const Entity2D* gold_e = find_entity_named(w.entities(), sample_scripts::kGoldName);
  const Entity2D* player_e = find_entity_named(w.entities(), "Player");
  if (!gate_e || !gold_e || !player_e || !gate_e->script || !gold_e->script ||
      gate_e->script->path != sample_scripts::kGate ||
      gold_e->script->path != sample_scripts::kGold || !gate_e->collider) {
    return fail("a new scene should seed the Corral Gate and Gold Nugget");
  }
  const std::uint64_t gate = gate_e->id;
  const std::uint64_t gold = gold_e->id;
  const std::uint64_t pid = player_e->id;
  if (scr.script_list().size() != 2 || !fs::exists(dir / sample_scripts::kGate) ||
      !fs::exists(dir / sample_scripts::kGold) ||
      !wire_has(wire, "Telegraph open for Script Smoke (2 scripts on file)")) {
    return fail("sample scripts on disk, in the Scripts list and on the wire");
  }

  // The Inspector reads the props a file declares.
  const editor::ScriptInfo gate_info = scr.script_info(sample_scripts::kGate);
  if (!gate_info.ok || gate_info.defaults.size() != 3 ||
      gate_info.defaults[0].name != "close_after" ||
      gate_info.defaults[1].name != "locked" ||
      gate_info.defaults[2].name != "open_line" ||
      gate_info.defaults[2].value.text != "The gate creaks open.") {
    return fail("gate.lua props (close_after, locked, open_line): " +
                gate_info.error);
  }

  // Prop override: one undo step, autosaved; undo / redo walk it.
  const std::size_t undo0 = w.undo_count();
  if (!scr.set_script_prop(gate, "open_line",
                           ScriptValue::of_text("Swing wide, partner.")) ||
      w.undo_count() != undo0 + 1 || w.undo_label() != "Script Prop: open_line" ||
      !has(read_text(scene), "Swing wide, partner.")) {
    return fail("prop override should be one undo step + autosave");
  }
  if (!scr.undo() || w.find(gate)->script->find("open_line") ||
      has(read_text(scene), "Swing wide, partner.") || !scr.redo() ||
      !w.find(gate)->script->find("open_line") ||
      !has(read_text(scene), "Swing wide, partner.")) {
    return fail("prop override undo / redo");
  }
  if (scr.set_script_prop(gate, "open_line",
                          ScriptValue::of_text("Swing wide, partner."))) {
    return fail("setting the same value again should be no step");
  }

  // Scripts panel: a new file from a template, never over an old one.
  std::string rel;
  if (!scr.create_script("Water Trough", "sign", &rel) ||
      rel != "scripts/water_trough.lua" || !fs::exists(dir / rel) ||
      scr.script_list().size() != 3 || !scr.script_info(rel).ok ||
      scr.create_script("water_trough", "blank") ||
      scr.create_script("!!!", "blank") ||
      scr.create_script("ghost", "no_such_template")) {
    return fail("create from a template (and refuse clobbers / bad names)");
  }
  // Attach / detach are undo steps that land in scene.json.
  ScriptData trough;
  trough.path = rel;
  if (!scr.set_script(pid, trough, "Attach Script") ||
      w.undo_label() != "Attach Script" ||
      !has(read_text(scene), "scripts/water_trough.lua") || !scr.undo() ||
      w.find(pid)->script || has(read_text(scene), "scripts/water_trough.lua")) {
    return fail("attach script undo + autosave");
  }

  // A script that blows up every tick rides on the Player: the error goes
  // on the wire once and only that instance is benched.
  write_text(dir / "scripts" / "boom.lua",
             "function on_tick(dt)\n  local saddle = nil\n"
             "  saddle.cinch = dt\nend\n");
  ScriptData boom;
  boom.path = "scripts/boom.lua";
  if (!scr.set_script(pid, boom, "Attach Script")) {
    return fail("attach boom.lua to the Player");
  }

  // --- Play ------------------------------------------------------------------
  const std::string disk_before = read_text(scene);
  const std::string doc_before = doc_text(w);
  if (!scr.start_play()) {
    return fail("play with scripts");
  }
  const runtime::World& world = scr.play_session().world();
  if (scr.set_script_prop(gate, "locked", ScriptValue::of_bool(true)) ||
      scr.set_script(gate, std::nullopt)) {
    return fail("script edits must be locked while playing");
  }
  // Left onto the nugget: pocketed, toasted, gone; the wire hears it all.
  for (int i = 0; i < 120 && world.find(gold); ++i) {
    scr.update_play(1.0 / 60.0, ride(-1.0f));
  }
  if (world.find(gold) || !w.find(gold) ||
      !wire_has(wire, "trigger: Player rode into Gold Nugget") ||
      !wire_has(wire, "toast: P1: Picked up 10 gold (10 in the purse)")) {
    return fail("the nugget should be pocketed with a trigger and toast line");
  }
  const editor::TelegraphLog::Line* gold_line =
      find_line(wire, [&](const runtime::LogEntry& e) {
        return e.file == sample_scripts::kGold && e.line == 15 &&
               has(e.text, "P1 picked up 10 gold at Gold Nugget");
      });
  if (!gold_line || gold_line->entry.entity != gold) {
    return fail("gold.lua:15 log line naming its entity");
  }
  // boom.lua: one error with file:line, then that instance stays quiet
  // while the nugget's script kept riding.
  std::size_t boom_errors = 0;
  for (const editor::TelegraphLog::Line& l : wire.lines()) {
    if (l.entry.level == runtime::LogLevel::Error &&
        l.entry.file == "scripts/boom.lua" && l.entry.line == 3 &&
        l.entry.entity == pid) {
      ++boom_errors;
    }
  }
  if (boom_errors != 1 || wire.unseen(runtime::LogLevel::Error) == 0 ||
      !world.find(pid)) {
    return fail("a script error should log boom.lua:3 once and bench only "
                "that instance (" + std::to_string(boom_errors) + " lines)");
  }
  // Clicking it picks the nugget out (it is still in the edit scene).
  if (!scr.focus_log_entry(gold_line->entry) || w.selected() == nullptr ||
      w.selected()->id != gold) {
    return fail("clicking a line should select its entity");
  }
  runtime::LogEntry stray = gold_line->entry;
  stray.entity = 999999;
  if (scr.focus_log_entry(stray)) {
    return fail("a line for a ride-only entity selects nothing");
  }

  // On to the shut gate.
  for (int i = 0; i < 90; ++i) {
    scr.update_play(1.0 / 60.0, ride(-1.0f));
  }
  const runtime::Actor* g = world.find(gate);
  if (!g || g->hidden || !g->data.collider) {
    return fail("the gate should still be shut");
  }
  // A quick tap on a frame too short for a tick still opens it on the next.
  if (scr.update_play(0.001, ride(0.0f, runtime::kButtonAction)) != 0) {
    return fail("a 1 ms frame should run no tick");
  }
  if (scr.update_play(1.0 / 60.0, ride(0.0f)) != 1) {
    return fail("the next frame should run one tick");
  }
  g = world.find(gate);
  if (!g || !g->hidden || g->data.collider ||
      !wire_has(wire, "scripts/gate.lua:32: Corral Gate opened by P1") ||
      !wire_has(wire, "toast: P1: Swing wide, partner.")) {
    return fail("the latched tap should open the gate with the Inspector's "
                "open_line");
  }

  // Hot reload: a save rides back in; a broken one keeps the old script and
  // lights the badge.
  const fs::path gate_file = dir / sample_scripts::kGate;
  const std::string gate_src = read_text(gate_file);
  write_text(gate_file, gate_src + "\nfunction on_tick(dt)\n  if not said then\n"
                                   "    said = true\n    log(\"fresh paint on \" "
                                   ".. self:name())\n  end\nend\n");
  if (scr.reload_play_scripts() < 1 ||
      !wire_has(wire, "Hot-reloaded 1 script mid-ride")) {
    return fail("a saved script should hot-reload mid-ride");
  }
  scr.update_play(1.0 / 60.0, ride(0.0f));
  if (!wire_has(wire, "fresh paint on Corral Gate")) {
    return fail("the reloaded on_tick should run");
  }
  wire.mark_seen();
  const std::size_t errors0 = wire.count(runtime::LogLevel::Error);
  write_text(gate_file, gate_src + "\nfunction on_tick(dt\n");
  scr.reload_play_scripts();
  scr.update_play(1.0 / 60.0, ride(0.0f));
  const editor::TelegraphLog::Line* broken =
      find_line(wire, [&](const runtime::LogEntry& e) {
        return e.level == runtime::LogLevel::Error &&
               e.file == sample_scripts::kGate && e.line > 0;
      });
  if (!broken || wire.count(runtime::LogLevel::Error) <= errors0 ||
      wire.unseen(runtime::LogLevel::Error) == 0 || !world.find(gate)) {
    return fail("a broken save should log file:line and light the badge");
  }
  write_text(gate_file, gate_src);

  // Stop: the gate, the nugget, the scene and scene.json are as they were.
  scr.stop_play();
  const Entity2D* gate_back = w.find(gate);
  if (doc_text(w) != doc_before || read_text(scene) != disk_before ||
      !w.find(gold) || !gate_back || !gate_back->collider ||
      !wire_has(wire, "Back at camp")) {
    return fail("stop should put back what the scripts changed");
  }
  // After Stop a click frames the nugget in the edit viewport.
  if (!scr.focus_log_entry(gold_line->entry) || w.selected()->id != gold ||
      w.pan_x() != w.find(gold)->center_x() ||
      w.pan_y() != w.find(gold)->center_y()) {
    return fail("focus should frame the entity after Stop");
  }

  // Console filters: levels, search, clear.
  wire.show_info = false;
  for (std::size_t i : wire.visible()) {
    if (wire.lines()[i].entry.level == runtime::LogLevel::Info) {
      return fail("info lines should hide when Info is off");
    }
  }
  wire.show_info = true;
  wire.search = "GOLD.LUA";
  const auto hits = wire.visible();
  for (std::size_t i : hits) {
    if (!has(runtime::format_log(wire.lines()[i].entry, true), "gold.lua")) {
      return fail("search should match case-insensitively");
    }
  }
  if (hits.empty()) {
    return fail("search should find the gold.lua line");
  }
  wire.search.clear();
  const std::size_t lines = wire.size();
  wire.clear();
  if (wire.size() != 0 || wire.count(runtime::LogLevel::Error) != 0 ||
      wire.unseen(runtime::LogLevel::Error) != 0) {
    return fail("clear should empty the wire and the badge");
  }

  scr.on_exit();
  fs::remove_all(dir, ec);
  std::cout << "[smoke] script editor OK (seeded gate + nugget, Inspector props "
               "undo / redo + autosave, template script + attach undo, Play: "
               "boom.lua:3 logged once + benched, nugget pocketed, gold.lua:15 "
               "click selects it, latched tap opens the gate with the "
               "Inspector's line, hot reload + broken save badge, Stop restores "
               "the scene and scene.json, Telegraph filters / search / clear "
               "over "
            << lines << " lines)\n";
  return 0;
}
