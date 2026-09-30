#pragma once

namespace ts {
namespace tombstone {

class Renderer {
 public:
  Renderer() = default;
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  bool init();
  void shutdown();
  void begin_frame();
  void end_frame();

 private:
  bool ready_ = false;
};

}  // namespace tombstone
}  // namespace ts
