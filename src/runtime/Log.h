#pragma once

// Telegraph: the one log the world, its scripts and the tools write to.
// The editor's Console panel shows it; ts_game prints it to stdout (and an
// optional log file). Plain data, no UI.

#include <cstdint>
#include <string>

namespace ts {
namespace tombstone {
namespace runtime {

enum class LogLevel : std::uint8_t { Info, Warn, Error };

// "info" / "warn" / "error".
inline const char* to_string(LogLevel level) {
  switch (level) {
    case LogLevel::Warn:
      return "warn";
    case LogLevel::Error:
      return "error";
    case LogLevel::Info:
    default:
      return "info";
  }
}

struct LogEntry {
  LogLevel level = LogLevel::Info;
  std::string channel;       // who wrote it: "script", "trigger", "editor"...
  std::string text;
  std::string file;          // project-relative script path ("" = none)
  int line = 0;              // 1-based line in `file` (0 = unknown)
  std::uint64_t entity = 0;  // entity it concerns (0 = none)
  std::uint64_t tick = 0;    // world tick (0 = outside the simulation)
};

// One line: "[error] scripts/gate.lua:12: attempt to call a nil value" or
// "[info] trigger: Rider rode into Gate"; "[tick 30] " up front when asked.
inline std::string format_log(const LogEntry& e, bool with_tick = false) {
  std::string s;
  if (with_tick && e.tick > 0) {
    s += "[tick " + std::to_string(e.tick) + "] ";
  }
  s += "[";
  s += to_string(e.level);
  s += "] ";
  if (!e.file.empty()) {
    s += e.file;
    if (e.line > 0) {
      s += ":" + std::to_string(e.line);
    }
    s += ": ";
  } else if (!e.channel.empty() && e.channel != "script") {
    s += e.channel + ": ";
  }
  s += e.text;
  return s;
}

}  // namespace runtime
}  // namespace tombstone
}  // namespace ts
