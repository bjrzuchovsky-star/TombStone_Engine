#include "net/ByteStream.h"

#include <cstring>
#include <limits>

namespace ts {
namespace tombstone {
namespace net {

static_assert(std::numeric_limits<float>::is_iec559,
              "the wire format sends IEEE 754 floats");

void ByteWriter::u16(std::uint16_t v) {
  u8(static_cast<std::uint8_t>(v & 0xFFu));
  u8(static_cast<std::uint8_t>(v >> 8));
}

void ByteWriter::u32(std::uint32_t v) {
  for (int i = 0; i < 4; ++i) {
    u8(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
  }
}

void ByteWriter::u64(std::uint64_t v) {
  for (int i = 0; i < 8; ++i) {
    u8(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
  }
}

void ByteWriter::f32(float v) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &v, sizeof bits);
  u32(bits);
}

void ByteWriter::varu(std::uint64_t v) {
  while (v >= 0x80u) {
    u8(static_cast<std::uint8_t>((v & 0x7Fu) | 0x80u));
    v >>= 7;
  }
  u8(static_cast<std::uint8_t>(v));
}

void ByteWriter::vari(std::int64_t v) {
  const std::uint64_t u = static_cast<std::uint64_t>(v);
  varu((u << 1) ^ (v < 0 ? ~std::uint64_t{0} : std::uint64_t{0}));
}

void ByteWriter::str(std::string_view s, std::size_t max_len) {
  const std::size_t n = s.size() < max_len ? s.size() : max_len;
  varu(n);
  bytes(reinterpret_cast<const std::uint8_t*>(s.data()), n);
}

void ByteWriter::bytes(const std::uint8_t* data, std::size_t size) {
  if (size > 0) {
    buf_.insert(buf_.end(), data, data + size);
  }
}

bool ByteReader::take(std::size_t n) {
  if (!ok_ || size_ - pos_ < n) {
    ok_ = false;
    return false;
  }
  return true;
}

std::uint8_t ByteReader::u8() {
  if (!take(1)) return 0;
  return data_[pos_++];
}

std::uint16_t ByteReader::u16() {
  if (!take(2)) return 0;
  const std::uint16_t v = static_cast<std::uint16_t>(
      data_[pos_] | (static_cast<std::uint16_t>(data_[pos_ + 1]) << 8));
  pos_ += 2;
  return v;
}

std::uint32_t ByteReader::u32() {
  if (!take(4)) return 0;
  std::uint32_t v = 0;
  for (int i = 0; i < 4; ++i) {
    v |= static_cast<std::uint32_t>(data_[pos_ + static_cast<std::size_t>(i)]) << (8 * i);
  }
  pos_ += 4;
  return v;
}

std::uint64_t ByteReader::u64() {
  if (!take(8)) return 0;
  std::uint64_t v = 0;
  for (int i = 0; i < 8; ++i) {
    v |= static_cast<std::uint64_t>(data_[pos_ + static_cast<std::size_t>(i)]) << (8 * i);
  }
  pos_ += 8;
  return v;
}

float ByteReader::f32() {
  const std::uint32_t bits = u32();
  float v = 0.0f;
  std::memcpy(&v, &bits, sizeof v);
  return v;
}

std::uint64_t ByteReader::varu() {
  std::uint64_t v = 0;
  for (int shift = 0; shift < 64; shift += 7) {
    const std::uint8_t b = u8();
    if (!ok_) return 0;
    v |= static_cast<std::uint64_t>(b & 0x7Fu) << shift;
    if ((b & 0x80u) == 0) {
      return v;
    }
  }
  ok_ = false;  // more than 10 bytes: not a varint we wrote
  return 0;
}

std::int64_t ByteReader::vari() {
  const std::uint64_t u = varu();
  return static_cast<std::int64_t>(u >> 1) ^ -static_cast<std::int64_t>(u & 1u);
}

std::string ByteReader::str(std::size_t max_len) {
  const std::uint64_t n = varu();
  if (!ok_ || n > max_len || !take(static_cast<std::size_t>(n))) {
    ok_ = false;
    return std::string();
  }
  std::string s(reinterpret_cast<const char*>(data_ + pos_),
                static_cast<std::size_t>(n));
  pos_ += static_cast<std::size_t>(n);
  return s;
}

std::uint64_t fnv1a64(const void* data, std::size_t size, std::uint64_t seed) {
  const auto* p = static_cast<const std::uint8_t*>(data);
  std::uint64_t h = seed;
  for (std::size_t i = 0; i < size; ++i) {
    h ^= p[i];
    h *= (std::uint64_t{1} << 40) + 0x1b3u;  // FNV-1a 64-bit prime
  }
  return h;
}

}  // namespace net
}  // namespace tombstone
}  // namespace ts
