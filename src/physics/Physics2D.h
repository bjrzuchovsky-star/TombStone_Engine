#pragma once

namespace ts {
namespace tombstone {

class Physics2D {
 public:
  Physics2D() = default;
  ~Physics2D() = default;

  Physics2D(const Physics2D&) = delete;
  Physics2D& operator=(const Physics2D&) = delete;

  bool init();
  void shutdown();
  void step(float delta_seconds);

 private:
  bool ready_ = false;
};

}  // namespace tombstone
}  // namespace ts
