#pragma once

// Headless --smoke checks for collision: scene.json v4 colliders and tile
// solidity, runtime movement resolution and triggers, and the editor's
// collision tools. Returns 0 when every check passes.
int run_collision_smoke();
