#pragma once

namespace ts {
namespace tombstone {

class Engine {
 public:
  Engine();
  ~Engine();

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  bool init();
  void shutdown();
  void tick(float delta_seconds);

  bool is_running() const { return running_; }

 private:
  bool running_ = false;
};

}  // namespace tombstone
}  // namespace ts
