#include "editor/workspace/SceneIO.h"
#include <string>
namespace ts { namespace tombstone { namespace editor { namespace scene_io {
bool load(Workspace2D&, const std::string& path, std::string* err) {
  if (err) *err = "stub load: " + path;
  return false;
}
bool save(const Workspace2D&, const std::string& path, std::string* err) {
  if (err) *err = "stub save: " + path;
  return false;
}
}}}}
