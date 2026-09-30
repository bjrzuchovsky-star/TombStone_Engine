#pragma once

#include <cstdint>
#include <string>

namespace ts {
namespace tombstone {

struct WindowDesc {
  std::string title = "TombStone";
  std::int32_t width = 1280;
  std::int32_t height = 720;
};

// Platform window stub - no GLFW (or other backend) wired yet.
class Window {
 public:
  Window() = default;
  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  bool create(const WindowDesc& desc);
  void destroy();
  void poll_events();

  bool is_open() const { return open_; }

 private:
  bool open_ = false;
  WindowDesc desc_{};
};

}  // namespace tombstone
}  // namespace ts
