#include "Renderer.h"

namespace ts {
namespace tombstone {

Renderer::~Renderer() {
  if (ready_) {
    shutdown();
  }
}

bool Renderer::init() {
  // Stub: graphics backend not selected yet.
  ready_ = true;
  return true;
}

void Renderer::shutdown() {
  ready_ = false;
}

void Renderer::begin_frame() {
  // Stub.
}

void Renderer::end_frame() {
  // Stub.
}

}  // namespace tombstone
}  // namespace ts
