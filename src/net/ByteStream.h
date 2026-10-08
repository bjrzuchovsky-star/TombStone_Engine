#pragma once

// Little-endian byte packing for the wire. Fixed-size integers, LEB128
// varints, zigzag for signed deltas, IEEE floats, length-prefixed strings.
// The reader never reads past its buffer: an overrun returns zeros and
// latches ok() to false, so a malformed packet is refused, never trusted.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ts {
namespace tombstone {
namespace net {

class ByteWriter {
 public:
  void u8(std::uint8_t v) { buf_.push_back(v); }
  void u16(std::uint16_t v);
  void u32(std::uint32_t v);
  void u64(std::uint64_t v);
  void f32(float v);
  // Unsigned LEB128 (1 byte below 128).
  void varu(std::uint64_t v);
  // Zigzag + LEB128: small magnitudes of either sign stay small.
  void vari(std::int64_t v);
  // varu length + bytes, cut at max_len bytes.
  void str(std::string_view s, std::size_t max_len = 4096);
  void bytes(const std::uint8_t* data, std::size_t size);

  const std::vector<std::uint8_t>& data() const { return buf_; }
  std::vector<std::uint8_t>& data() { return buf_; }
  std::size_t size() const { return buf_.size(); }
  void clear() { buf_.clear(); }

 private:
  std::vector<std::uint8_t> buf_;
};

class ByteReader {
 public:
  ByteReader(const std::uint8_t* data, std::size_t size)
      : data_(data), size_(size) {}
  explicit ByteReader(const std::vector<std::uint8_t>& v)
      : data_(v.data()), size_(v.size()) {}

  std::uint8_t u8();
  std::uint16_t u16();
  std::uint32_t u32();
  std::uint64_t u64();
  float f32();
  std::uint64_t varu();
  std::int64_t vari();
  // Fails (empty string, ok() false) when longer than max_len.
  std::string str(std::size_t max_len = 4096);

  bool ok() const { return ok_; }
  std::size_t remaining() const { return ok_ ? size_ - pos_ : 0; }
  bool at_end() const { return ok_ && pos_ == size_; }
  void fail() { ok_ = false; }

 private:
  bool take(std::size_t n);

  const std::uint8_t* data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t pos_ = 0;
  bool ok_ = true;
};

// FNV-1a 64-bit offset basis, built from its two 32-bit halves.
inline constexpr std::uint64_t kFnvOffsetBasis =
    (std::uint64_t{0xcbf29ce4u} << 32) | std::uint64_t{0x84222325u};

// 64-bit FNV-1a: scene hashes in the handshake (not cryptographic).
std::uint64_t fnv1a64(const void* data, std::size_t size,
                      std::uint64_t seed = kFnvOffsetBasis);

}  // namespace net
}  // namespace tombstone
}  // namespace ts
