// Runtime scripting checks: hook order on the fixed tick, trigger and
// action-button hooks, the script API, timers, error containment, the
// sandbox, hot reload and replayability. Driven headless on World.

#include "SmokeScripting.h"

#include "runtime/World.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace ts::tombstone;
using namespace ts::tombstone::runtime;

int fail(const std::string& what) {
  std::cerr << "[smoke] script runtime FAILED: " << what << '\n';
  return 1;
}

Entity2D ent(std::uint64_t id, const std::string& name, float x, float y,
             float w = 32.0f, float h = 32.0f) {
  Entity2D e;
  e.id = id;
  e.name = name;
  e.x = x;
  e.y = y;
  e.w = w;
  e.h = h;
  return e;
}

Entity2D scripted(std::uint64_t id, const std::string& name, float x, float y,
                  const std::string& path) {
  Entity2D e = ent(id, name, x, y);
  e.script = ScriptData{};
  e.script->path = path;
  return e;
}

Entity2D rider(float x, float y) {
  Entity2D e = ent(1, "Rider", x, y);
  e.player = PlayerControllerData{};
  e.player->speed = 120.0f;  // 2 px a tick
  e.collider = default_collider(e);
  return e;
}

InputFrame input(bool right, bool action) {
  InputFrame in;
  in.slot(0) = digital_input(false, right, false, false);
  if (action) in.slot(0).buttons |= kButtonAction;
  return in;
}

// Step `ticks` times, collecting script + trigger lines.
void ride(World& w, int ticks, const InputFrame& in, std::vector<LogEntry>* out) {
  for (int i = 0; i < ticks; ++i) {
    w.step(in);
    for (LogEntry& e : w.take_logs()) out->push_back(std::move(e));
  }
}

std::vector<std::string> lines(const std::vector<LogEntry>& logs,
                               const std::string& channel = "script") {
  std::vector<std::string> out;
  for (const LogEntry& e : logs) {
    if (e.channel == channel && e.level == LogLevel::Info) out.push_back(e.text);
  }
  return out;
}

bool has(const std::vector<std::string>& v, const std::string& s) {
  return std::find(v.begin(), v.end(), s) != v.end();
}

const LogEntry* first_error(const std::vector<LogEntry>& logs) {
  for (const LogEntry& e : logs) {
    if (e.level == LogLevel::Error) return &e;
  }
  return nullptr;
}

std::string dump(const std::vector<std::string>& v) {
  std::string s;
  for (const std::string& l : v) s += "\n  " + l;
  return s;
}

// on_start before on_tick, both on the fixed tick; top level at build.
int check_hook_order() {
  World w;
  w.script_library().put("scripts/order.lua",
                         "props = { label = 'clock', step = 1 }\n"
                         "local n = 0\n"
                         "log('load', tick(), props.label)\n"
                         "function on_start() log('start', tick(), props.label, props.step) end\n"
                         "function on_tick(dt)\n"
                         "  n = n + 1\n"
                         "  if n <= 2 then log('tick', tick(), dt == 1 / 60) end\n"
                         "end\n");
  Entity2D clock = scripted(2, "Clock", 0, 0, "scripts/order.lua");
  clock.script->set("label", ScriptValue::of_text("override"));
  std::string err;
  if (!w.build({clock}, "", &err)) return fail("build: " + err);
  std::vector<LogEntry> logs = w.take_logs();
  ride(w, 3, InputFrame{}, &logs);
  const std::vector<std::string> want = {"load 0 clock", "start 1 override 1",
                                         "tick 1 true", "tick 2 true"};
  if (lines(logs) != want) return fail("hook order:" + dump(lines(logs)));
  if (logs[1].file != "scripts/order.lua" || logs[1].line != 4 ||
      logs[1].entity != 2 || logs[1].tick != 1) {
    return fail("log() should carry file:line, entity and tick");
  }
  return 0;
}

// Trigger enter / exit reach the trigger's script once each; the action
// button pokes the nearest interactable in reach once per press.
int check_trigger_and_interact() {
  World w;
  w.script_library().put("scripts/zone.lua",
                         "function on_trigger_enter(o) log('enter', o:name(), o:slot()) end\n"
                         "function on_trigger_exit(o) log('exit', o:name()) end\n");
  w.script_library().put("scripts/lever.lua",
                         "function on_interact(r)\n"
                         "  self:set('pulls', self:get('pulls', 0) + 1)\n"
                         "  log('pulled by', r:name(), r:is_player())\n"
                         "end\n");
  Entity2D zone = scripted(2, "Zone", 60, 0, "scripts/zone.lua");
  zone.w = 20;
  zone.collider = ColliderData{};
  zone.collider->w = 20;
  zone.collider->h = 32;
  zone.collider->trigger = true;
  Entity2D lever = scripted(3, "Lever", 0, 50, "scripts/lever.lua");  // 18 px below
  Entity2D far = scripted(4, "Far Lever", 0, 200, "scripts/lever.lua");
  std::string err;
  if (!w.build({rider(0, 0), zone, lever, far}, "", &err)) return fail("build: " + err);
  std::vector<LogEntry> logs;
  // Press, hold, release, press again: two pulls, the near lever only.
  ride(w, 1, input(false, true), &logs);
  ride(w, 2, input(false, true), &logs);
  ride(w, 1, input(false, false), &logs);
  ride(w, 1, input(false, true), &logs);
  const ScriptValue* pulls = w.script_get(3, "pulls");
  if (!pulls || pulls->number != 2.0 || w.script_get(4, "pulls") ||
      lines(logs) != std::vector<std::string>{"pulled by Rider true",
                                              "pulled by Rider true"}) {
    return fail("interact:" + dump(lines(logs)));
  }
  if (w.interact_target(1) != 3) return fail("interact target should be the near lever");
  logs.clear();
  ride(w, 60, input(true, false), &logs);  // 120 px right: through the zone
  const std::vector<std::string> s = lines(logs);
  const std::vector<std::string> t = lines(logs, "trigger");
  if (s != std::vector<std::string>{"enter Rider 1", "exit Rider"} ||
      t != std::vector<std::string>{"Rider rode into Zone", "Rider rode out of Zone"}) {
    return fail("trigger hooks:" + dump(s) + dump(t));
  }
  return 0;
}

// The API: position, visibility, collider, state, spawn / duplicate /
// destroy, toasts, find / player, timers (after / every / cancel).
int check_api_and_timers() {
  World w;
  w.script_library().put(
      "scripts/api.lua",
      "function on_start()\n"
      "  self:set_pos(210, 5)\n"
      "  local x, y = self:pos()\n"
      "  log('pos', x, y)\n"
      "  self:move_by(-10, -5)\n"
      "  self:hide()\n"
      "  self:set_collider(false)\n"
      "  log('flags', self:visible(), self:collider_on())\n"
      "  self:set('word', 'crate'); self:set('n', 3); self:set('ok', true)\n"
      "  log('state', self:get('word'), self:get('n'), self:get('ok'), self:get('none', 'dflt'))\n"
      "  after(0.5, function() log('after', tick()) end)\n"
      "  local k, id = 0, nil\n"
      "  id = every(0.25, function() k = k + 1; log('every', tick()); if k == 3 then cancel(id) end end)\n"
      "  local c = spawn('Coin', 300, 40)\n"
      "  log('spawned', c:name(), c:spawned(), c ~= self, c == find('Coin') or c:spawned())\n"
      "  local d = duplicate(self, 400, 0)\n"
      "  d:destroy()\n"
      "  log('dup', d:alive(), spawn('Nobody', 0, 0) == nil)\n"
      "  toast('Howdy', 1)\n"
      "  player(1):toast('Just you', 1)\n"
      "  log('find', find('Coin') ~= nil, find('Nobody') == nil, player(2) == nil)\n"
      "end\n");
  w.script_library().put("scripts/coin.lua",
                         "function on_start() log('coin', self:spawned()) end\n");
  Entity2D box = scripted(2, "Box", 200, 0, "scripts/api.lua");
  box.collider = default_collider(box);
  Entity2D coin = scripted(3, "Coin", 500, 0, "scripts/coin.lua");
  std::string err;
  if (!w.build({rider(0, 0), box, coin}, "", &err)) return fail("build: " + err);
  std::vector<LogEntry> logs;
  ride(w, 1, InputFrame{}, &logs);
  const Actor* b = w.find(2);
  if (!b || b->data.x != 200.0f || b->data.y != 0.0f || !b->hidden ||
      b->data.collider || !b->stashed || w.actors().size() != 4 ||
      w.toasts_for(0).size() != 2 || w.toasts_for(1).size() != 1) {
    return fail("API effects on the world");
  }
  std::vector<DrawQuad> quads;
  w.build_draw_list(WorldRect{-1000, -1000, 1000, 1000}, 1.0f, nullptr, &quads);
  for (const DrawQuad& q : quads) {
    if (q.entity == 2) return fail("hidden entity drew");
  }
  ride(w, 61, InputFrame{}, &logs);  // toasts set on tick 1 show through 61
  const std::vector<std::string> want = {
      "pos 210.0 5.0", "flags false false", "state crate 3 true dflt",
      "spawned Coin true true true", "dup false true",
      "find true true true", "coin false", "coin true",
      "every 16", "after 31", "every 31", "every 46"};
  std::vector<std::string> got = lines(logs);
  // Box starts first (scene order), then the authored coin, then the coin
  // Box spawned (appended, same start pass).
  if (got != want) return fail("API lines:" + dump(got));
  if (w.scripts()->timer_count() != 0 || !w.toasts().empty()) {
    return fail("timers should be done and toasts expired");
  }
  if (lines(logs, "toast") != std::vector<std::string>{"all riders: Howdy", "P1: Just you"}) {
    return fail("toasts should be logged");
  }
  return 0;
}

// A runtime error is logged with file:line, stops that instance only.
// Endless loops and memory hogs are cut off the same way.
int check_errors() {
  World w;
  w.script_library().put("scripts/bad.lua",
                         "function on_tick(dt)\n"
                         "  if tick() == 3 then\n"
                         "    local t = nil\n"
                         "    return t.boom\n"
                         "  end\n"
                         "end\n");
  w.script_library().put("scripts/loop.lua", "function on_start() while true do end end\n");
  w.script_library().put("scripts/hog.lua",
                         "function on_start()\n"
                         "  local t = {}\n"
                         "  for i = 1, 1e7 do t[i] = string.rep('x', 1000) .. i end\n"
                         "end\n");
  w.script_library().put("scripts/syntax.lua", "function on_start( log('x') end\n");
  w.script_library().put("scripts/steady.lua", "function on_tick() self:set('t', tick()) end\n");
  std::string err;
  if (!w.build({scripted(2, "Bad", 0, 0, "scripts/bad.lua"),
                scripted(3, "Loop", 40, 0, "scripts/loop.lua"),
                scripted(4, "Hog", 80, 0, "scripts/hog.lua"),
                scripted(5, "Syntax", 120, 0, "scripts/syntax.lua"),
                scripted(6, "Steady", 160, 0, "scripts/steady.lua"),
                scripted(7, "Missing", 200, 0, "scripts/missing.lua")},
               "", &err)) {
    return fail("build: " + err);
  }
  std::vector<LogEntry> logs = w.take_logs();
  ride(w, 10, InputFrame{}, &logs);
  ScriptHost& h = *w.scripts();
  const ScriptValue* t = w.script_get(6, "t");
  if (h.running(2) || h.running(3) || h.running(4) || h.running(5) ||
      h.running(7) || !h.running(6) || !t || t->number != 10.0) {
    return fail("errors should stop only the failing scripts");
  }
  auto error_for = [&](std::uint64_t id) -> const LogEntry* {
    for (const LogEntry& e : logs) {
      if (e.level == LogLevel::Error && e.entity == id) return &e;
    }
    return nullptr;
  };
  const LogEntry* bad = error_for(2);
  const LogEntry* loop = error_for(3);
  const LogEntry* hog = error_for(4);
  const LogEntry* syn = error_for(5);
  const LogEntry* miss = error_for(7);
  if (!bad || bad->file != "scripts/bad.lua" || bad->line != 4 || bad->tick != 3 ||
      bad->text.find("attempt to index a nil value") == std::string::npos) {
    return fail("runtime error file:line: " + (bad ? format_log(*bad) : "none"));
  }
  if (!loop || loop->file != "scripts/loop.lua" || loop->line != 1 ||
      loop->text.find("ran too long") == std::string::npos) {
    return fail("endless loop: " + (loop ? format_log(*loop) : "none"));
  }
  if (!hog || hog->text.find("not enough memory") == std::string::npos ||
      h.memory_used() > ScriptHost::kMemoryLimit) {
    return fail("memory cap: " + (hog ? format_log(*hog) : "none"));
  }
  if (!syn || syn->file != "scripts/syntax.lua" || syn->line != 1 || !miss ||
      miss->file != "scripts/missing.lua") {
    return fail("compile / missing file errors");
  }
  return 0;
}

// No io / os / load / require / debug; no bytecode; strings locked.
int check_sandbox() {
  World w;
  w.script_library().put(
      "scripts/sandbox.lua",
      "local leaks = {}\n"
      "for _, n in ipairs({'io', 'os', 'require', 'dofile', 'loadfile', 'load',\n"
      "                    'loadstring', 'debug', 'package', 'collectgarbage', '_G'}) do\n"
      "  if _ENV[n] ~= nil then leaks[#leaks + 1] = n end\n"
      "end\n"
      "log('leaks', #leaks == 0 and 'none' or table.concat(leaks, ','))\n"
      "log('locked', string.dump == nil, getmetatable('') , getmetatable(_ENV))\n"
      "log('protected', pcall(setmetatable, _ENV, nil))\n"
      "function on_start() local f = io.open('scene.json') end\n");
  std::string err;
  if (!w.build({scripted(2, "Sneak", 0, 0, "scripts/sandbox.lua")}, "", &err)) {
    return fail("build: " + err);
  }
  std::vector<LogEntry> logs = w.take_logs();
  ride(w, 1, InputFrame{}, &logs);
  const std::vector<std::string> got = lines(logs);
  const LogEntry* e = first_error(logs);
  if (got.size() < 3 || got[0] != "leaks none" || got[1] != "locked true locked false" ||
      got[2].rfind("protected false", 0) != 0 || !e || e->line != 9 ||
      e->text.find("global 'io'") == std::string::npos) {
    return fail("sandbox:" + dump(got));
  }
  return 0;
}

// Hot reload from memory and from disk; a broken save keeps the old code;
// a fixed file brings a stopped script back.
int check_hot_reload() {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / "ts_script_smoke";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir / "scripts", ec);
  auto write = [&](const std::string& text) {
    std::ofstream(dir / "scripts" / "disk.lua", std::ios::binary) << text;
  };
  write("function on_tick() self:set('v', 'disk one') end\n");
  World w;
  w.script_library().put("scripts/hot.lua",
                         "props = { msg = 'v1' }\n"
                         "function on_tick() self:set('v', props.msg) end\n");
  std::string err;
  if (!w.build({scripted(2, "Hot", 0, 0, "scripts/hot.lua"),
                scripted(3, "Disk", 40, 0, "scripts/disk.lua")},
               dir.string(), &err)) {
    return fail("build: " + err);
  }
  std::vector<LogEntry> logs;
  ride(w, 2, InputFrame{}, &logs);
  auto text = [&](std::uint64_t id) {
    const ScriptValue* v = w.script_get(id, "v");
    return v ? v->text : std::string("(none)");
  };
  if (text(2) != "v1" || text(3) != "disk one") return fail("before reload");
  w.script_library().put("scripts/hot.lua",
                         "props = { msg = 'v2' }\n"
                         "function on_tick() self:set('v', props.msg) end\n"
                         "function on_reload() log('reloaded', self:get('v')) end\n");
  write("function on_tick() self:set('v', 'disk two, longer') end\n");
  if (w.reload_scripts() != 2) return fail("two files should reload");
  ride(w, 1, InputFrame{}, &logs);
  if (text(2) != "v2" || text(3) != "disk two, longer" ||
      !has(lines(logs), "reloaded v1")) {
    return fail("after reload: " + text(2) + " / " + text(3) + dump(lines(logs)));
  }
  // Broken save: logged, old code keeps riding.
  logs.clear();
  w.script_library().put("scripts/hot.lua", "function on_tick( oops\n");
  w.reload_scripts();
  ride(w, 1, InputFrame{}, &logs);
  const LogEntry* e = first_error(logs);
  if (!w.scripts()->running(2) || !e || e->file != "scripts/hot.lua" ||
      e->text.find("previous version keeps riding") == std::string::npos) {
    return fail("broken reload should keep the old version");
  }
  // A crashed script comes back when its file is fixed.
  w.script_library().put("scripts/hot.lua", "function on_tick() error('down') end\n");
  w.reload_scripts();
  ride(w, 1, InputFrame{}, &logs);
  if (w.scripts()->running(2)) return fail("crash after reload should stop it");
  w.script_library().put("scripts/hot.lua", "function on_tick() self:set('v', 'back') end\n");
  w.reload_scripts();
  ride(w, 1, InputFrame{}, &logs);
  if (!w.scripts()->running(2) || text(2) != "back") return fail("fixed file should revive");
  fs::remove_all(dir, ec);
  return 0;
}

// Same scene + same input = same log (pairs order, math.random, timers).
int check_replay() {
  const std::string src =
      "local t = { alpha = 1, bravo = 2, charlie = 3, delta = 4, echo = 5 }\n"
      "function on_tick()\n"
      "  if tick() % 7 == 0 then\n"
      "    local s = ''\n"
      "    for k in pairs(t) do s = s .. k:sub(1, 1) end\n"
      "    log(s, math.random(1, 1000), math.random())\n"
      "  end\n"
      "end\n";
  std::vector<std::string> runs[2];
  for (auto& run : runs) {
    World w;
    w.script_library().put("scripts/dice.lua", src);
    std::string err;
    if (!w.build({rider(0, 0), scripted(2, "Dice", 0, 60, "scripts/dice.lua")}, "", &err)) {
      return fail("build: " + err);
    }
    std::vector<LogEntry> logs;
    ride(w, 50, input(true, false), &logs);
    run = lines(logs);
  }
  if (runs[0].size() != 7 || runs[0] != runs[1]) return fail("replay differs");
  std::vector<ScriptProp> defs;
  std::string err;
  if (!ScriptHost::describe("props = { reach = 24, line = 'Howdy', locked = false, t = {} }\n"
                            "function on_start() end\n",
                            "scripts/x.lua", &defs, &err) ||
      defs.size() != 3 || defs[0].name != "line" || defs[1].name != "locked" ||
      defs[2].name != "reach" || defs[2].value.number != 24.0 ||
      ScriptHost::describe("props = {\n  reach = ,\n}\n", "scripts/x.lua", &defs, &err) ||
      err.find("scripts/x.lua:2:") != 0) {
    return fail("describe props: " + err);
  }
  return 0;
}

}  // namespace

int run_script_runtime_smoke() {
  if (check_hook_order() != 0 || check_trigger_and_interact() != 0 ||
      check_api_and_timers() != 0 || check_errors() != 0 ||
      check_sandbox() != 0 || check_hot_reload() != 0 || check_replay() != 0) {
    return 1;
  }
  std::cout << "[smoke] script runtime OK (Lua 5.4 sandbox: on_start/on_tick on the "
               "60 Hz tick, trigger enter/exit + action-button interact, pos/hide/"
               "collider/state/spawn/duplicate/destroy/toast API, after/every/cancel "
               "timers, errors logged with file:line and contained (nil index, endless "
               "loop, 64 MB cap, syntax, missing file), io/os/load blocked, hot reload "
               "memory + disk + broken save kept + revive, replayable pairs/random, "
               "prop defaults)\n";
  return 0;
}
