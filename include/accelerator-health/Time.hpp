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

#include "accelerator-health/Wire.hpp"

#include <cstdint>

namespace ah {

// All time-driven production behavior is driven through an injectable Clock so
// that tests are deterministic and never depend on wall-clock sleeps.

using DurationNanos = std::int64_t;

struct Timestamp {
  std::int64_t nanos{0};
  friend bool operator==(const Timestamp&, const Timestamp&) = default;
  friend bool operator<(const Timestamp& a, const Timestamp& b) noexcept { return a.nanos < b.nanos; }
  friend bool operator<=(const Timestamp& a, const Timestamp& b) noexcept { return a.nanos <= b.nanos; }
  friend bool operator>(const Timestamp& a, const Timestamp& b) noexcept { return a.nanos > b.nanos; }
  friend bool operator>=(const Timestamp& a, const Timestamp& b) noexcept { return a.nanos >= b.nanos; }
  Timestamp operator+(DurationNanos d) const { return Timestamp{nanos + d}; }
  DurationNanos operator-(const Timestamp& o) const { return nanos - o.nanos; }
  void write(wire::Writer& w) const { w.i64(nanos); }
  static Timestamp read(wire::Reader& r) noexcept { return Timestamp{r.i64()}; }
};

inline constexpr DurationNanos kMs = 1'000'000;
inline constexpr DurationNanos kSec = 1'000'000'000;

class Clock {
 public:
  virtual ~Clock() = default;
  virtual Timestamp now() const = 0;
};

// Wall/process clock backed by std::chrono.
class SystemClock final : public Clock {
 public:
  Timestamp now() const override;
};

// Deterministic clock used by tests and adversarial scenarios.
class FakeClock final : public Clock {
 public:
  explicit FakeClock(std::int64_t start = 0) : now_(start) {}
  Timestamp now() const override { return Timestamp{now_}; }
  void set(Timestamp t) { now_ = t.nanos; }
  void advance(DurationNanos d) { now_ += d; }
  std::int64_t value() const { return now_; }

 private:
  std::int64_t now_;
};

}  // namespace ah
