#pragma once

// json_mini moved to core/JsonMini.h so the scene loader and the runtime
// can share it without depending on the editor. Editor code keeps using
// editor::json_mini through this alias.
#include "core/JsonMini.h"

namespace ts {
namespace tombstone {
namespace editor {
namespace json_mini = ::ts::tombstone::json_mini;
}  // namespace editor
}  // namespace tombstone
}  // namespace ts
