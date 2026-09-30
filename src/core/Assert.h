#pragma once

#include <cstdlib>
#include <iostream>

namespace ts {
namespace tombstone {

#if defined(NDEBUG)
#define TS_ASSERT(cond) ((void)0)
#else
#define TS_ASSERT(cond)                                                          \
  do {                                                                           \
    if (!(cond)) {                                                               \
      std::cerr << "TS_ASSERT failed: " << #cond << " (" << __FILE__ << ':'     \
                << __LINE__ << ")\n";                                            \
      std::abort();                                                              \
    }                                                                            \
  } while (0)
#endif

}  // namespace tombstone
}  // namespace ts
