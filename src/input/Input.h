#pragma once

namespace ts {
namespace tombstone {

class Input {
 public:
  Input() = default;
  ~Input() = default;

  Input(const Input&) = delete;
  Input& operator=(const Input&) = delete;

  bool init();
  void shutdown();
  void update();

 private:
  bool ready_ = false;
};

}  // namespace tombstone
}  // namespace ts
