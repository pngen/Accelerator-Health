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
#include <functional>
#include <limits>
#include <string>

namespace ah {

// Strong, separately-typed identities and generations. A StrongId identifies an
// individual object instance; a Generation is a monotonic authority counter that
// can roll forward independently of other generations. Each is a distinct C++
// type keyed by a tag type, so unrelated identities/generations are never
// interchangeable.

template <class Tag>
class StrongId {
 public:
  using TagType = Tag;
  using ValueType = std::uint64_t;

  constexpr StrongId() noexcept = default;
  constexpr explicit StrongId(std::uint64_t v) noexcept : value_(v) {}


  constexpr std::uint64_t get() const noexcept { return value_; }
  constexpr explicit operator bool() const noexcept { return value_ != 0; }
  constexpr bool is_null() const noexcept { return value_ == 0; }

  StrongId& operator++() noexcept { ++value_; return *this; }
  StrongId next() const noexcept { return StrongId(value_ + 1); }

  std::string string() const { return std::to_string(value_); }

  friend bool operator==(const StrongId&, const StrongId&) = default;
  friend bool operator<(const StrongId& a, const StrongId& b) noexcept { return a.value_ < b.value_; }
  friend bool operator<=(const StrongId& a, const StrongId& b) noexcept { return a.value_ <= b.value_; }
  friend bool operator>(const StrongId& a, const StrongId& b) noexcept { return a.value_ > b.value_; }
  friend bool operator>=(const StrongId& a, const StrongId& b) noexcept { return a.value_ >= b.value_; }

  void write(wire::Writer& w) const { w.u64(value_); }
  static StrongId read(wire::Reader& r) noexcept { return StrongId(r.u64()); }

 private:
  std::uint64_t value_ = 0;
};

template <class Tag>
class Generation {
 public:
  using TagType = Tag;
  using ValueType = std::uint64_t;

  constexpr Generation() noexcept = default;
  constexpr explicit Generation(std::uint64_t v) noexcept : value_(v) {}

  constexpr std::uint64_t get() const noexcept { return value_; }
  constexpr bool is_null() const noexcept { return value_ == 0; }

  Generation& operator++() {
    if (value_ == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("generation overflow");
    ++value_;
    return *this;
  }
  Generation next() const {
    if (value_ == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("generation overflow");
    return Generation(value_ + 1);
  }

  std::string string() const { return std::to_string(value_); }

  friend bool operator==(const Generation&, const Generation&) = default;
  friend bool operator<(const Generation& a, const Generation& b) noexcept { return a.value_ < b.value_; }
  friend bool operator<=(const Generation& a, const Generation& b) noexcept { return a.value_ <= b.value_; }
  friend bool operator>(const Generation& a, const Generation& b) noexcept { return a.value_ > b.value_; }
  friend bool operator>=(const Generation& a, const Generation& b) noexcept { return a.value_ >= b.value_; }

  void write(wire::Writer& w) const { w.u64(value_); }
  static Generation read(wire::Reader& r) noexcept { return Generation(r.u64()); }

 private:
  std::uint64_t value_ = 0;
};

#define AH_DEFINE_ID(NAME)                      struct NAME##Tag { };                         using NAME = StrongId<NAME##Tag>;             static_assert(true, "")

#define AH_DEFINE_GENERATION(NAME)              struct NAME##Tag { };                         using NAME = Generation<NAME##Tag>;           static_assert(true, "")

// Strong object identities.
AH_DEFINE_ID(AcceleratorId);
AH_DEFINE_ID(DeviceUuid);
AH_DEFINE_ID(NodeId);
AH_DEFINE_ID(WorkerId);
AH_DEFINE_ID(WorkerBootId);
AH_DEFINE_ID(ObservationId);
AH_DEFINE_ID(HealthAssessmentId);
AH_DEFINE_ID(ValidationRunId);
AH_DEFINE_ID(FaultId);
AH_DEFINE_ID(IncidentId);
AH_DEFINE_ID(QuarantineId);
AH_DEFINE_ID(RecoveryId);
AH_DEFINE_ID(PolicyId);

// Separately-typed authority generations. These do not roll together.
AH_DEFINE_GENERATION(HealthGeneration);
AH_DEFINE_GENERATION(ObservationGeneration);
AH_DEFINE_GENERATION(ValidationGeneration);
AH_DEFINE_GENERATION(FaultGeneration);
AH_DEFINE_GENERATION(IncidentGeneration);
AH_DEFINE_GENERATION(RecoveryGeneration);
AH_DEFINE_GENERATION(DeviceGeneration);
AH_DEFINE_GENERATION(PolicyGeneration);
AH_DEFINE_GENERATION(WorkerGeneration);

#undef AH_DEFINE_ID
#undef AH_DEFINE_GENERATION

// Authority envelope attached to every distributed health report. Every field is
// strongly typed so a rollover of one generation cannot be mistaken for another.
struct AuthorityEnvelope {
  std::uint64_t coordinatorEpoch = 0;
  WorkerId workerId{};
  WorkerBootId workerBootId{};
  ObservationGeneration observationGeneration{};
  HealthGeneration healthGeneration{};
  ValidationGeneration validationGeneration{};
  DeviceGeneration deviceGeneration{};

  friend bool operator==(const AuthorityEnvelope&, const AuthorityEnvelope&) = default;

  void write(wire::Writer& w) const {
    w.u64(coordinatorEpoch);
    workerId.write(w);
    workerBootId.write(w);
    observationGeneration.write(w);
    healthGeneration.write(w);
    validationGeneration.write(w);
    deviceGeneration.write(w);
  }
  static AuthorityEnvelope read(wire::Reader& r) {
    AuthorityEnvelope e;
    e.coordinatorEpoch = r.u64();
    e.workerId = WorkerId::read(r);
    e.workerBootId = WorkerBootId::read(r);
    e.observationGeneration = ObservationGeneration::read(r);
    e.healthGeneration = HealthGeneration::read(r);
    e.validationGeneration = ValidationGeneration::read(r);
    e.deviceGeneration = DeviceGeneration::read(r);
    return e;
  }
};

namespace idsuffix {
using namespace ::ah;
}  // namespace idsuffix

}  // namespace ah

namespace std {
template <class Tag>
struct hash<ah::StrongId<Tag>> {
  std::size_t operator()(const ah::StrongId<Tag>& id) const noexcept {
    return std::hash<std::uint64_t>{}(id.get());
  }
};
template <class Tag>
struct hash<ah::Generation<Tag>> {
  std::size_t operator()(const ah::Generation<Tag>& g) const noexcept {
    return std::hash<std::uint64_t>{}(g.get());
  }
};
}  // namespace std
