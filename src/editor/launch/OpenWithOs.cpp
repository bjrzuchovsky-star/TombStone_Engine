#include "editor/launch/OpenWithOs.h"

#include <filesystem>
#include <string>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#else
#include <cerrno>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace ts {
namespace tombstone {
namespace editor {
namespace launcher {

namespace fs = std::filesystem;

bool open_with_os(const std::string& path, std::string* error_out) {
  std::error_code ec;
  if (path.empty() || !fs::exists(fs::path(path), ec)) {
    if (error_out) *error_out = "nothing to open at " + path;
    return false;
  }
#if defined(_WIN32)
  const std::wstring w = fs::path(path).wstring();
  const HINSTANCE r =
      ShellExecuteW(nullptr, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  if (reinterpret_cast<INT_PTR>(r) <= 32) {
    if (error_out) {
      *error_out = "ShellExecute failed (code " +
                   std::to_string(static_cast<long long>(
                       reinterpret_cast<INT_PTR>(r))) +
                   "); is there an app for .lua files?";
    }
    return false;
  }
  return true;
#else
#if defined(__APPLE__)
  const char* tool = "open";
#else
  const char* tool = "xdg-open";
#endif
  // Double fork (like spawn_game): the opener is adopted by init and the
  // editor never waits on it.
  const pid_t child = fork();
  if (child < 0) {
    if (error_out) *error_out = "fork failed";
    return false;
  }
  if (child == 0) {
    const pid_t grandchild = fork();
    if (grandchild == 0) {
      const char* argv[] = {tool, path.c_str(), nullptr};
      execvp(tool, const_cast<char* const*>(argv));
      _exit(127);
    }
    _exit(grandchild < 0 ? 1 : 0);
  }
  int status = 0;
  pid_t waited = 0;
  do {
    waited = waitpid(child, &status, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    if (error_out) *error_out = std::string("could not start ") + tool;
    return false;
  }
  return true;
#endif
}

}  // namespace launcher
}  // namespace editor
}  // namespace tombstone
}  // namespace ts
