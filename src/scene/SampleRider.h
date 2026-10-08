#pragma once

// The sample cowboy: a small, procedurally drawn 4-direction idle / walk
// sheet (PNG) plus its .anim.json, written into a project's assets so
// animation works before anyone draws a pixel. Generated in code, so it is
// ours to ship (no third-party art) and identical on every machine.

#include "scene/Animation.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ts {
namespace tombstone {
namespace sample_rider {

inline constexpr const char* kImage = "assets/rider.png";
inline constexpr const char* kSet = "assets/rider.anim.json";
inline constexpr int kFrameW = 32;
inline constexpr int kFrameH = 48;
inline constexpr int kCols = 6;  // idle 0..1, walk 2..5
inline constexpr int kRows = 4;  // down, up, left, right

// RGBA8 pixels of the sheet (kCols * kFrameW x kRows * kFrameH).
std::vector<std::uint8_t> pixels(int* width, int* height);
// idle_<dir> (2 frames, 3 fps) and walk_<dir> (4 frames, 8 fps) per row.
AnimSet anim_set();

// Writes kImage and kSet under project_dir unless they already exist (a
// project's own edits are never overwritten). True when both are present.
bool write_into_project(const std::string& project_dir,
                        std::string* error_out = nullptr);

}  // namespace sample_rider

// Minimal PNG encoder (RGBA8, fixed-Huffman deflate with run matches).
// Fine for flat pixel art; zero dependencies.
std::vector<std::uint8_t> encode_png_rgba(int width, int height,
                                          const std::vector<std::uint8_t>& rgba);

}  // namespace tombstone
}  // namespace ts
