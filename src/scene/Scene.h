#pragma once

namespace ts {
namespace tombstone {

class Scene {
 public:
  Scene() = default;
  ~Scene() = default;

  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;

  bool init();
  void shutdown();
  void update(float delta_seconds);

 private:
  bool ready_ = false;
};

}  // namespace tombstone
}  // namespace ts
