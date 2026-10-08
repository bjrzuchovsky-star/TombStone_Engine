#pragma once

#include <string>

namespace ts {
namespace tombstone {
namespace editor {
namespace launcher {

// Hand a file to whatever the OS opens it with (ShellExecute on Windows,
// `open` on macOS, `xdg-open` elsewhere) and return without waiting.
// False + reason when it could not even be started.
bool open_with_os(const std::string& path, std::string* error_out = nullptr);

}  // namespace launcher
}  // namespace editor
}  // namespace tombstone
}  // namespace ts
