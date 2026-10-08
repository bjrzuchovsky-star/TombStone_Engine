#pragma once

// Headless --smoke checks for sprite animation: .anim.json sets and clip
// timing, scene.json v5 animators, runtime clip picking and the editor's
// animation tools. Returns 0 when every check passes.
int run_animation_smoke();
