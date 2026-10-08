// Lua's lstate.c, compiled with a fixed string-hash seed.
//
// Stock Lua mixes the clock and a few addresses into the seed, which makes
// table traversal order (pairs) differ between runs. Scripts are part of the
// server-authoritative simulation, so the same scene + inputs must replay the
// same way everywhere: one constant seed for every state.
#define luai_makeseed(L) (static_cast<unsigned int>(0x7E3B5A11u))
#include "lstate.c"
