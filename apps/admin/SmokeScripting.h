#pragma once

// Headless --smoke checks for gameplay scripting: scene.json v6 script
// components, then the Lua runtime. Returns 0 when every check passes.
int run_scripting_smoke();
// SmokeScriptRuntime.cpp: hooks, API, timers, errors, sandbox, reload.
int run_script_runtime_smoke();
