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

#include "accelerator-health/Util.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ah::wire {

// Bounded, deterministic, little-endian binary serialization primitives.
// Uses fixed byte order, explicit length bounds, and error-state reporting so
// that malformed, truncated, or oversized input is rejected deterministically
// rather than delivered as partially-valid data.

class Writer {
 public:
  Writer() = default;
  explicit Writer(std::size_t reserve) { buf_.reserve(reserve); }

  void u8(std::uint8_t v) { buf_.push_back(v); }
  void u16(std::uint16_t v) {
    const std::size_t n = buf_.size();
    buf_.resize(n + 2);
    util::store_le(v, buf_.data() + n);
  }
  void u32(std::uint32_t v) {
    const std::size_t n = buf_.size();
    buf_.resize(n + 4);
    util::store_le(v, buf_.data() + n);
  }
  void u64(std::uint64_t v) {
    const std::size_t n = buf_.size();
    buf_.resize(n + 8);
    util::store_le(v, buf_.data() + n);
  }
  void i32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }
  void i64(std::int64_t v) { u64(static_cast<std::uint64_t>(v)); }
  void f32(float v) {
    std::uint32_t bits;
    static_assert(sizeof(bits) == sizeof(v));
    std::memcpy(&bits, &v, sizeof(bits));
    u32(bits);
  }
  void f64(double v) {
    std::uint64_t bits;
    static_assert(sizeof(bits) == sizeof(v));
    std::memcpy(&bits, &v, sizeof(bits));
    u64(bits);
  }

  void bytes(const std::uint8_t* data, std::size_t len) {
    if (len == 0) return;
    buf_.insert(buf_.end(), data, data + len);
  }
  void bytes(std::span<const std::uint8_t> s) { bytes(s.data(), s.size()); }

  // Length-prefixed string with explicit bound.
  void string(std::string_view s) {
    if (s.size() > util::kMaxString) throw std::runtime_error("wire: string too long");
    u32(static_cast<std::uint32_t>(s.size()));
    if (!s.empty()) bytes(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
  }
  void string(const std::string& s) { string(std::string_view(s)); }

  // Raw span append (used by frames that carry a payload verbatim).
  void raw(std::span<const std::uint8_t> s) { buf_.insert(buf_.end(), s.begin(), s.end()); }

  const std::vector<std::uint8_t>& data() const& { return buf_; }
  std::vector<std::uint8_t>&& take() && { return std::move(buf_); }
  std::size_t size() const { return buf_.size(); }

 private:
  std::vector<std::uint8_t> buf_;
};

class Reader {
 public:
  explicit Reader(std::span<const std::uint8_t> data) : data_(data) {}
  explicit Reader(const std::vector<std::uint8_t>& data) : data_(data) {}

  bool ok() const { return !fail_; }
  bool fail() const { return fail_; }
  std::size_t position() const { return pos_; }
  std::size_t remaining() const { return data_.size() - pos_; }
  std::span<const std::uint8_t> rest() const { return data_.subspan(pos_); }

  std::uint8_t u8() noexcept {
    if (remaining() < 1) { fail_ = true; return 0; }
    return data_[pos_++];
  }
  std::uint16_t u16() noexcept {
    if (remaining() < 2) { fail_ = true; return 0; }
    const auto v = util::load_le16(data_.data() + pos_); pos_ += 2; return v;
  }
  std::uint32_t u32() noexcept {
    if (remaining() < 4) { fail_ = true; return 0; }
    const auto v = util::load_le32(data_.data() + pos_); pos_ += 4; return v;
  }
  std::uint64_t u64() noexcept {
    if (remaining() < 8) { fail_ = true; return 0; }
    const auto v = util::load_le64(data_.data() + pos_); pos_ += 8; return v;
  }
  std::int32_t i32() noexcept { return static_cast<std::int32_t>(u32()); }
  std::int64_t i64() noexcept { return static_cast<std::int64_t>(u64()); }
  float f32() noexcept {
    const auto bits = u32();
    float v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
  }
  double f64() noexcept {
    const auto bits = u64();
    double v;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
  }

  // Reads a length then that many bytes. Rejects length > cap.
  bool bytes(std::uint8_t* out, std::size_t cap, std::size_t* actual = nullptr) noexcept {
    const auto len = u32();
    if (fail_ || len > cap || len > remaining()) { fail_ = true; return false; }
    if (len > 0 && out) std::memcpy(out, data_.data() + pos_, len);
    pos_ += len;
    if (actual) *actual = len;
    return true;
  }
  std::vector<std::uint8_t> bytes_vec(std::uint32_t cap = util::kMaxCount) noexcept {
    const auto len = u32();
    if (fail_ || len > cap || len > remaining()) { fail_ = true; return {}; }
    std::vector<std::uint8_t> v(data_.data() + pos_, data_.data() + pos_ + len);
    pos_ += len;
    return v;
  }
  // Reads a length-prefixed string. Rejects length > cap.
  std::string string(std::uint32_t cap = util::kMaxString) noexcept {
    const auto len = u32();
    if (fail_ || len > cap || len > remaining()) { fail_ = true; return {}; }
    std::string s(reinterpret_cast<const char*>(data_.data() + pos_), len);
    pos_ += len;
    return s;
  }

  // Consumes a raw bounded span, returning its bounds.
  bool skip(std::size_t n) noexcept {
    if (remaining() < n) { fail_ = true; return false; }
    pos_ += n;
    return true;
  }

  void set_fail() noexcept { fail_ = true; }

 private:
  std::span<const std::uint8_t> data_;
  std::size_t pos_ = 0;
  bool fail_ = false;
};

}  // namespace ah::wire
