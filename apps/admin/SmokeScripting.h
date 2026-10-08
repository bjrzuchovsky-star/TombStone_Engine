#pragma once

// Headless --smoke checks for gameplay scripting: scene.json v6 script
// components, the Lua runtime, then the editor's scripting tools. Returns 0
// when every check passes.
int run_scripting_smoke();
// SmokeScriptRuntime.cpp: hooks, API, timers, errors, sandbox, reload.
int run_script_runtime_smoke();
// SmokeScriptEditor.cpp: Inspector props, Scripts panel, Telegraph, Play /
// Stop with scripts, hot reload.
int run_script_editor_smoke();
