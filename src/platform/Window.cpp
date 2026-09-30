#include "Window.h"

namespace ts {
namespace tombstone {

Window::~Window() {
  if (open_) {
    destroy();
  }
}

bool Window::create(const WindowDesc& desc) {
  // Stub: no native window backend yet (GLFW deferred).
  desc_ = desc;
  open_ = true;
  return true;
}

void Window::destroy() {
  open_ = false;
}

void Window::poll_events() {
  // Stub: poll platform events when a backend exists.
}

}  // namespace tombstone
}  // namespace ts
