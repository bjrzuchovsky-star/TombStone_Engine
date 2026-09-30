#include "Scene.h"

namespace ts {
namespace tombstone {

bool Scene::init() {
  // Stub: entity/scene graph scaffolding later.
  ready_ = true;
  return true;
}

void Scene::shutdown() {
  ready_ = false;
}

void Scene::update(float /*delta_seconds*/) {
  // Stub: update scene nodes / systems.
}

}  // namespace tombstone
}  // namespace ts
