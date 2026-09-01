// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace ah::util {

// ---- Integer boundaries used to reject malformed wire data ----
inline constexpr std::size_t kMaxBytes = (std::size_t{1} << 24);       // 16 MiB frame / value cap
inline constexpr std::uint32_t kMaxCount = (std::uint32_t{1} << 20);   // 1,048,576 elements
inline constexpr std::uint32_t kMaxString = (std::uint32_t{1} << 20);  // 1 MiB string

// ---- CRC-32 (IEEE 802.3, reflected) ----
inline std::uint32_t crc32(const std::uint8_t* data, std::size_t len) noexcept {
  static const std::array<std::uint32_t, 256> table = [] {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1u)) : (c >> 1u);
      t[i] = c;
    }
    return t;
  }();
  std::uint32_t crc = 0xFFFFFFFFu;
  for (std::size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8u);
  return crc ^ 0xFFFFFFFFu;
}

inline std::uint32_t crc32(std::string_view sv) noexcept {
  return crc32(reinterpret_cast<const std::uint8_t*>(sv.data()), sv.size());
}

// ---- Little-endian fixed-width values ----
inline void store_le(std::uint16_t v, std::uint8_t* p) noexcept {
  p[0] = static_cast<std::uint8_t>(v & 0xFFu);
  p[1] = static_cast<std::uint8_t>((v >> 8u) & 0xFFu);
}
inline void store_le(std::uint32_t v, std::uint8_t* p) noexcept {
  p[0] = static_cast<std::uint8_t>(v & 0xFFu);
  p[1] = static_cast<std::uint8_t>((v >> 8u) & 0xFFu);
  p[2] = static_cast<std::uint8_t>((v >> 16u) & 0xFFu);
  p[3] = static_cast<std::uint8_t>((v >> 24u) & 0xFFu);
}
inline void store_le(std::uint64_t v, std::uint8_t* p) noexcept {
  for (int i = 0; i < 8; ++i) p[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu);
}
inline std::uint16_t load_le16(const std::uint8_t* p) noexcept {
  return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8u);
}
inline std::uint32_t load_le32(const std::uint8_t* p) noexcept {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8u) |
         (static_cast<std::uint32_t>(p[2]) << 16u) | (static_cast<std::uint32_t>(p[3]) << 24u);
}
inline std::uint64_t load_le64(const std::uint8_t* p) noexcept {
  std::uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v |= (static_cast<std::uint64_t>(p[i]) << (8 * i));
  return v;
}

// ---- Hex helpers ----
inline char hex_digit(std::uint8_t b) noexcept { return "0123456789abcdef"[b & 0x0Fu]; }
inline std::string to_hex(const std::uint8_t* data, std::size_t len) {
  std::string out;
  out.reserve(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    out.push_back(hex_digit(static_cast<std::uint8_t>(data[i] >> 4u)));
    out.push_back(hex_digit(data[i]));
  }
  return out;
}
inline std::string to_hex(std::string_view sv) {
  return to_hex(reinterpret_cast<const std::uint8_t*>(sv.data()), sv.size());
}

// ---- Deterministic PRNG (splitmix64). Fixed-seed tests print seed. ----
class SplitMix64 {
 public:
  explicit SplitMix64(std::uint64_t seed) : state_(seed) {}
  std::uint64_t next() noexcept {
    std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30u)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27u)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31u);
  }
  std::uint64_t state() const noexcept { return state_; }

 private:
  std::uint64_t state_;
};

}  // namespace ah::util
