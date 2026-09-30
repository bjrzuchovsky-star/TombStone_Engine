#pragma once

namespace ts {
namespace tombstone {

// Networking stub. Target: 2D four-player MMO later.
class Net {
 public:
  Net() = default;
  ~Net() = default;

  Net(const Net&) = delete;
  Net& operator=(const Net&) = delete;

  bool init();
  void shutdown();
  void update();

 private:
  bool ready_ = false;
};

}  // namespace tombstone
}  // namespace ts
