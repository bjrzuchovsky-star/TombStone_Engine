#pragma once

// Headless --smoke checks for sprite animation: .anim.json sets and clip
// timing, scene.json v5 animators, runtime clip picking and the editor's
// animation tools. Returns 0 when every check passes.
int run_animation_smoke();
// The editor half (SmokeAnimEditor.cpp); run_animation_smoke() calls it.
int run_animation_editor_smoke();
