#include "scene/SampleRider.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace ts {
namespace tombstone {

namespace fs = std::filesystem;

// --- PNG ------------------------------------------------------------------------

namespace {

std::uint32_t crc32(const std::uint8_t* data, std::size_t n,
                    std::uint32_t crc = 0) {
  static const std::array<std::uint32_t, 256> table = [] {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t c = i;
      for (int k = 0; k < 8; ++k) {
        c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      }
      t[i] = c;
    }
    return t;
  }();
  crc = ~crc;
  for (std::size_t i = 0; i < n; ++i) {
    crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
  }
  return ~crc;
}

void put_u32(std::vector<std::uint8_t>* out, std::uint32_t v) {
  out->push_back(static_cast<std::uint8_t>(v >> 24));
  out->push_back(static_cast<std::uint8_t>(v >> 16));
  out->push_back(static_cast<std::uint8_t>(v >> 8));
  out->push_back(static_cast<std::uint8_t>(v));
}

void put_chunk(std::vector<std::uint8_t>* out, const char type[4],
               const std::vector<std::uint8_t>& body) {
  put_u32(out, static_cast<std::uint32_t>(body.size()));
  const std::size_t start = out->size();
  out->insert(out->end(), type, type + 4);
  out->insert(out->end(), body.begin(), body.end());
  put_u32(out, crc32(out->data() + start, out->size() - start));
}


// Bit writer for deflate (LSB first; Huffman codes go in MSB first).
class Bits {
 public:
  explicit Bits(std::vector<std::uint8_t>* out) : out_(out) {}
  void put(std::uint32_t v, int n) {
    for (int i = 0; i < n; ++i) bit((v >> i) & 1u);
  }
  void code(std::uint32_t c, int n) {
    for (int i = n - 1; i >= 0; --i) bit((c >> i) & 1u);
  }
  void flush() {
    if (used_ > 0) out_->push_back(acc_);
    acc_ = 0;
    used_ = 0;
  }

 private:
  void bit(std::uint32_t b) {
    acc_ = static_cast<std::uint8_t>(acc_ | (b << used_));
    if (++used_ == 8) flush();
  }
  std::vector<std::uint8_t>* out_;
  std::uint8_t acc_ = 0;
  int used_ = 0;
};

void put_literal(Bits& bw, int v) {
  if (v < 144) {
    bw.code(0x30u + static_cast<std::uint32_t>(v), 8);
  } else if (v < 256) {
    bw.code(0x190u + static_cast<std::uint32_t>(v - 144), 9);
  } else if (v < 280) {
    bw.code(static_cast<std::uint32_t>(v - 256), 7);
  } else {
    bw.code(0xC0u + static_cast<std::uint32_t>(v - 280), 8);
  }
}

// One fixed-Huffman deflate block. Matches only look back one pixel, one
// byte or one scanline: enough for flat pixel art, a few lines of code.
void deflate_fixed(const std::vector<std::uint8_t>& in, std::size_t stride,
                   std::vector<std::uint8_t>* out) {
  static const int kLenBase[29] = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                   15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                   67, 83, 99, 115, 131, 163, 195, 227, 258};
  static const int kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
                                    1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
                                    4, 4, 4, 4, 5, 5, 5, 5, 0};
  static const int kDistBase[30] = {
      1,    2,    3,    4,    5,    7,     9,     13,    17,  25,
      33,   49,   65,   97,   129,  193,   257,   385,   513, 769,
      1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
  Bits bw(out);
  bw.put(1, 1);  // final block
  bw.put(1, 2);  // fixed Huffman
  const std::size_t dists[3] = {4, 1, stride};
  std::size_t i = 0;
  while (i < in.size()) {
    std::size_t best_len = 0;
    std::size_t best_dist = 0;
    for (std::size_t d : dists) {
      if (d == 0 || d > i || d > 32768) continue;
      std::size_t n = 0;
      while (n < 258 && i + n < in.size() && in[i + n] == in[i + n - d]) ++n;
      if (n > best_len) {
        best_len = n;
        best_dist = d;
      }
    }
    if (best_len < 3) {
      put_literal(bw, in[i]);
      ++i;
      continue;
    }
    int lc = 28;
    while (kLenBase[lc] > static_cast<int>(best_len)) --lc;
    put_literal(bw, 257 + lc);
    bw.put(static_cast<std::uint32_t>(static_cast<int>(best_len) - kLenBase[lc]),
           kLenExtra[lc]);
    int dc = 29;
    while (kDistBase[dc] > static_cast<int>(best_dist)) --dc;
    bw.code(static_cast<std::uint32_t>(dc), 5);
    const int dextra = dc < 4 ? 0 : dc / 2 - 1;
    bw.put(static_cast<std::uint32_t>(static_cast<int>(best_dist) - kDistBase[dc]),
           dextra);
    i += best_len;
  }
  put_literal(bw, 256);  // end of block
  bw.flush();
}

}  // namespace

std::vector<std::uint8_t> encode_png_rgba(int width, int height,
                                          const std::vector<std::uint8_t>& rgba) {
  std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  if (width <= 0 || height <= 0 ||
      rgba.size() < static_cast<std::size_t>(width) * height * 4) {
    return {};
  }
  std::vector<std::uint8_t> ihdr;
  put_u32(&ihdr, static_cast<std::uint32_t>(width));
  put_u32(&ihdr, static_cast<std::uint32_t>(height));
  ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});  // 8-bit RGBA, no interlace
  put_chunk(&png, "IHDR", ihdr);

  // Scanlines with filter 0, one fixed-Huffman zlib stream.
  const std::size_t row = static_cast<std::size_t>(width) * 4;
  std::vector<std::uint8_t> raw;
  raw.reserve((row + 1) * static_cast<std::size_t>(height));
  for (int y = 0; y < height; ++y) {
    raw.push_back(0);
    const auto* src = rgba.data() + row * static_cast<std::size_t>(y);
    raw.insert(raw.end(), src, src + row);
  }
  std::vector<std::uint8_t> z = {0x78, 0x01};
  deflate_fixed(raw, row + 1, &z);
  std::uint32_t a = 1;
  std::uint32_t b = 0;
  for (std::uint8_t v : raw) {
    a = (a + v) % 65521u;
    b = (b + a) % 65521u;
  }
  put_u32(&z, (b << 16) | a);
  put_chunk(&png, "IDAT", z);
  put_chunk(&png, "IEND", {});
  return png;
}

// --- The cowboy -------------------------------------------------------------------

namespace sample_rider {

namespace {

struct Rgba {
  std::uint8_t r, g, b, a;
};

// Frontier palette.
constexpr Rgba kHat{120, 78, 42, 255};
constexpr Rgba kHatDark{82, 52, 28, 255};
constexpr Rgba kBand{46, 30, 18, 255};
constexpr Rgba kSkin{232, 182, 140, 255};
constexpr Rgba kSkinShade{196, 142, 104, 255};
constexpr Rgba kHair{92, 58, 30, 255};
constexpr Rgba kEye{36, 24, 18, 255};
constexpr Rgba kBandana{188, 48, 40, 255};
constexpr Rgba kShirt{226, 206, 160, 255};
constexpr Rgba kVest{140, 70, 40, 255};
constexpr Rgba kVestDark{104, 50, 30, 255};
constexpr Rgba kStar{246, 204, 72, 255};
constexpr Rgba kBelt{60, 38, 22, 255};
constexpr Rgba kJeans{62, 90, 140, 255};
constexpr Rgba kJeansDark{46, 66, 106, 255};
constexpr Rgba kBoot{70, 42, 24, 255};
constexpr Rgba kSpur{210, 210, 220, 255};
constexpr Rgba kOutline{34, 22, 16, 255};
constexpr Rgba kShadow{0, 0, 0, 70};

class Canvas {
 public:
  Canvas(int w, int h) : w_(w), h_(h), px_(static_cast<std::size_t>(w) * h * 4, 0) {}

  void rect(int ox, int oy, int x, int y, int w, int h, Rgba c) {
    for (int j = y; j < y + h; ++j) {
      for (int i = x; i < x + w; ++i) {
        if (i < 0 || j < 0 || i >= kFrameW || j >= kFrameH) continue;
        set(ox + i, oy + j, c);
      }
    }
  }
  void set(int x, int y, Rgba c) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    std::uint8_t* p = &px_[(static_cast<std::size_t>(y) * w_ + x) * 4];
    p[0] = c.r;
    p[1] = c.g;
    p[2] = c.b;
    p[3] = c.a;
  }
  Rgba get(int x, int y) const {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return Rgba{0, 0, 0, 0};
    const std::uint8_t* p = &px_[(static_cast<std::size_t>(y) * w_ + x) * 4];
    return Rgba{p[0], p[1], p[2], p[3]};
  }
  // Ink a 1 px outline around the solid pixels of one cell.
  void outline(int ox, int oy) {
    std::vector<std::pair<int, int>> ink;
    for (int y = 0; y < kFrameH; ++y) {
      for (int x = 0; x < kFrameW; ++x) {
        if (get(ox + x, oy + y).a != 0) continue;
        bool edge = false;
        const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& v : d) {
          const int nx = x + v[0];
          const int ny = y + v[1];
          if (nx < 0 || ny < 0 || nx >= kFrameW || ny >= kFrameH) continue;
          if (get(ox + nx, oy + ny).a == 255) edge = true;
        }
        if (edge) ink.emplace_back(ox + x, oy + y);
      }
    }
    for (const auto& p : ink) set(p.first, p.second, kOutline);
  }
  std::vector<std::uint8_t>& data() { return px_; }

 private:
  int w_;
  int h_;
  std::vector<std::uint8_t> px_;
};

// One cell. row: 0 down, 1 up, 2 left, 3 right. col: 0..1 idle, 2..5 walk.
void draw_frame(Canvas& cv, int row, int col) {
  const int ox = col * kFrameW;
  const int oy = row * kFrameH;
  const bool walk = col >= 2;
  const int step = walk ? col - 2 : 0;          // 0..3
  const int bob = walk ? (step % 2 == 1 ? -1 : 0) : (col == 1 ? 1 : 0);
  const int swing = walk ? (step == 0 ? 2 : step == 2 ? -2 : 0) : 0;
  const bool side = row >= 2;
  const bool left = row == 2;
  auto R = [&](int x, int y, int w, int h, Rgba c) {
    // Mirror the right-facing drawing for left.
    const int xx = left ? kFrameW - x - w : x;
    cv.rect(ox, oy, xx, y, w, h, c);
  };

  // Ground shadow (never outlined: alpha < 255).
  cv.rect(ox, oy, 9, 45, 14, 2, kShadow);
  cv.rect(ox, oy, 11, 44, 10, 1, kShadow);

  const int b = bob;  // upper body offset
  if (!side) {
    // Legs + boots: the lifted foot is drawn shorter.
    const int lift_l = walk && step == 0 ? 2 : 0;
    const int lift_r = walk && step == 2 ? 2 : 0;
    R(11, 33 + b, 4, 9 - lift_l, kJeans);
    R(17, 33 + b, 4, 9 - lift_r, kJeans);
    R(14, 33 + b, 1, 7, kJeansDark);
    R(10, 42 - lift_l, 5, 3, kBoot);
    R(17, 42 - lift_r, 5, 3, kBoot);
    R(10, 44 - lift_l, 1, 1, kSpur);
    R(21, 44 - lift_r, 1, 1, kSpur);
    // Arms swing opposite the legs.
    R(7, 20 + b + (swing > 0 ? -1 : swing < 0 ? 1 : 0), 3, 10, row == 0 ? kShirt : kVestDark);
    R(22, 20 + b + (swing > 0 ? 1 : swing < 0 ? -1 : 0), 3, 10, row == 0 ? kShirt : kVestDark);
    R(7, 30 + b + (swing > 0 ? -1 : swing < 0 ? 1 : 0), 3, 2, kSkin);
    R(22, 30 + b + (swing > 0 ? 1 : swing < 0 ? -1 : 0), 3, 2, kSkin);
    // Torso: open vest over the shirt (front) or the vest's back.
    if (row == 0) {
      R(10, 19 + b, 12, 12, kShirt);
      R(10, 19 + b, 4, 12, kVest);
      R(18, 19 + b, 4, 12, kVest);
      R(12, 23 + b, 1, 1, kStar);
      R(11, 24 + b, 3, 1, kStar);
      R(12, 25 + b, 1, 1, kStar);
    } else {
      R(10, 19 + b, 12, 12, kVest);
      R(15, 20 + b, 2, 10, kVestDark);
    }
    R(10, 31 + b, 12, 2, kBelt);
    if (row == 0) R(15, 31 + b, 2, 2, kStar);
    // Head.
    if (row == 0) {
      R(11, 8 + b, 10, 9, kSkin);
      R(11, 15 + b, 10, 2, kSkinShade);
      R(13, 12 + b, 1, 2, kEye);
      R(18, 12 + b, 1, 2, kEye);
      R(10, 17 + b, 12, 2, kBandana);
      R(15, 19 + b, 2, 2, kBandana);
    } else {
      R(11, 8 + b, 10, 9, kHair);
      R(11, 15 + b, 10, 2, kSkinShade);
      R(10, 17 + b, 12, 2, kBandana);
    }
    // Hat: crown, band, wide brim.
    R(10, 1 + b, 12, 5, kHat);
    R(11, 1 + b, 10, 1, kHatDark);
    R(10, 5 + b, 12, 1, kBand);
    R(5, 6 + b, 22, 2, row == 0 ? kHat : kHatDark);
  } else {
    // Side view (drawn facing right; mirrored for left).
    const int fwd = swing * 3 / 2;  // stride: front leg reach
    R(14 - fwd, 33 + b, 4, 9, kJeansDark);   // back leg
    R(14 - fwd, 42, 5, 3, kHatDark);
    R(13 + fwd, 33 + b, 4, 9, kJeans);       // front leg
    R(13 + fwd, 42, 6, 3, kBoot);
    R(12 + fwd, 44, 1, 1, kSpur);
    // Torso + back arm behind, front arm swinging.
    R(12, 19 + b, 9, 12, kVest);
    R(19, 19 + b, 2, 12, kShirt);
    R(12, 31 + b, 9, 2, kBelt);
    R(15 - swing / 2, 20 + b, 3, 10, kVestDark);
    R(15 - swing / 2, 30 + b, 3, 2, kSkin);
    // Head in profile: nose forward, hair behind.
    R(12, 8 + b, 9, 9, kSkin);
    R(12, 8 + b, 3, 8, kHair);
    R(21, 12 + b, 1, 2, kSkin);
    R(18, 11 + b, 1, 2, kEye);
    R(12, 15 + b, 9, 2, kSkinShade);
    R(12, 17 + b, 9, 2, kBandana);
    // Hat: brim further out the front.
    R(11, 1 + b, 10, 5, kHat);
    R(11, 1 + b, 10, 1, kHatDark);
    R(11, 5 + b, 10, 1, kBand);
    R(7, 6 + b, 19, 2, kHat);
  }
  cv.outline(ox, oy);
}

}  // namespace

std::vector<std::uint8_t> pixels(int* width, int* height) {
  const int w = kCols * kFrameW;
  const int h = kRows * kFrameH;
  Canvas cv(w, h);
  for (int r = 0; r < kRows; ++r) {
    for (int c = 0; c < kCols; ++c) {
      draw_frame(cv, r, c);
    }
  }
  if (width) *width = w;
  if (height) *height = h;
  return std::move(cv.data());
}

AnimSet anim_set() {
  AnimSet set;
  set.image = "rider.png";
  set.grid = AnimGrid{kFrameW, kFrameH, kCols, kRows};
  const char* dirs[kRows] = {"down", "up", "left", "right"};
  for (int r = 0; r < kRows; ++r) {
    AnimClip idle;
    idle.name = std::string("idle_") + dirs[r];
    idle.start = r * kCols;
    idle.count = 2;
    idle.fps = 3.0f;
    set.clips.push_back(idle);
  }
  for (int r = 0; r < kRows; ++r) {
    AnimClip walk;
    walk.name = std::string("walk_") + dirs[r];
    walk.start = r * kCols + 2;
    walk.count = 4;
    walk.fps = 8.0f;
    set.clips.push_back(walk);
  }
  set.default_clip = "idle_down";
  set.normalize();
  return set;
}

bool write_into_project(const std::string& project_dir,
                        std::string* error_out) {
  if (project_dir.empty()) {
    if (error_out) *error_out = "no project folder for the sample rider";
    return false;
  }
  const fs::path image = fs::path(project_dir) / kImage;
  const fs::path set = fs::path(project_dir) / kSet;
  std::error_code ec;
  fs::create_directories(image.parent_path(), ec);
  if (!fs::exists(image, ec)) {
    int w = 0;
    int h = 0;
    const std::vector<std::uint8_t> rgba = pixels(&w, &h);
    const std::vector<std::uint8_t> bytes = encode_png_rgba(w, h, rgba);
    std::ofstream out(image, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    if (!out) {
      if (error_out) *error_out = "Could not write " + image.string();
      return false;
    }
  }
  if (!fs::exists(set, ec)) {
    if (!anim_json::save_file(set.string(), anim_set(), error_out)) {
      return false;
    }
  }
  return true;
}

}  // namespace sample_rider
}  // namespace tombstone
}  // namespace ts
