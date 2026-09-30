#include "core/Engine.h"

#include <iostream>

int main() {
  using namespace ts::tombstone;

  std::cout << "TombStone Client (stub)\n";

  Engine engine;
  if (!engine.init()) {
    std::cerr << "Engine init failed\n";
    return 1;
  }

  engine.tick(1.0f / 60.0f);
  engine.shutdown();
  return 0;
}
