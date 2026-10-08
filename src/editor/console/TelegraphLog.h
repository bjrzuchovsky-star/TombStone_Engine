#pragma once

// The editor's Telegraph: every line the running world sends (scripts,
// toasts, trigger events) plus the editor's own status notes, kept for the
// Console panel. Plain data and filtering, no ImGui, so --smoke drives it
// headless.

#include "runtime/Log.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace editor {

class TelegraphLog {
 public:
  static constexpr std::size_t kMaxLines = 5000;  // oldest drop off first

  struct Line {
    runtime::LogEntry entry;
    std::uint64_t seq = 0;  // 1, 2, 3... for the life of the log
  };

  void add(runtime::LogEntry entry);
  void add(runtime::LogLevel level, std::string channel, std::string text);
  void clear();

  const std::deque<Line>& lines() const { return lines_; }
  std::size_t size() const { return lines_.size(); }
  std::uint64_t total() const { return seq_; }  // lines ever added
  // Lines of one level still in the log.
  std::size_t count(runtime::LogLevel level) const;
  // Status-bar badge: errors / warnings since the panel was last looked at.
  std::size_t unseen(runtime::LogLevel level) const;
  void mark_seen();

  // Console filter: level toggles plus a case-insensitive search over the
  // formatted line ("[warn] scripts/gate.lua:12: ...").
  bool show_info = true;
  bool show_warn = true;
  bool show_error = true;
  std::string search;
  bool passes(const Line& line) const;
  // Indexes into lines() that pass, oldest first.
  std::vector<std::size_t> visible() const;

 private:
  static std::size_t slot(runtime::LogLevel level);

  std::deque<Line> lines_;
  std::uint64_t seq_ = 0;
  std::array<std::size_t, 3> counts_{};
  std::array<std::size_t, 3> unseen_{};
};

}  // namespace editor
}  // namespace tombstone
}  // namespace ts
