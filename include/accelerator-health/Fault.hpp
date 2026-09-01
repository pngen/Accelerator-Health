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

#include "accelerator-health/Evidence.hpp"

namespace ah {

enum class ResolutionState : std::uint8_t {
  OPEN = 0,
  RESOLVED = 1,
  SUPPRESSED = 2,
};
inline constexpr std::array<std::pair<ResolutionState, const char*>, 3> kResolutionStateNames = {{
  {ResolutionState::OPEN, "OPEN"}, {ResolutionState::RESOLVED, "RESOLVED"},
  {ResolutionState::SUPPRESSED, "SUPPRESSED"},
}};
constexpr const char* to_string(ResolutionState e) noexcept { return enum_to_string(kResolutionStateNames, e); }
constexpr std::optional<ResolutionState> parse_resolution(std::string_view s) noexcept { return enum_from_name(kResolutionStateNames, s); }
constexpr std::optional<ResolutionState> resolution_from_byte(std::uint8_t b) noexcept { return enum_from_byte(kResolutionStateNames, b); }
inline void write(wire::Writer& w, ResolutionState e) { w.u8(static_cast<std::uint8_t>(static_cast<int>(e))); }
inline bool read(wire::Reader& r, ResolutionState& e) noexcept {
  auto v = resolution_from_byte(r.u8());
  if (!r.ok() || !v) { r.set_fail(); return false; }
  e = *v; return true;
}

// A typed fault with full provenance and resolution state. Append-only history
// preserves every fault even after recovery.
struct Fault {
  FaultId id{};
  AcceleratorId accelerator{};
  FaultType type = FaultType::UNKNOWN_FAULT;
  FaultClass faultClass = FaultClass::UNKNOWN;
  Severity severity = Severity::INFO;
  Timestamp timestamp{};
  ObservationGeneration observationGeneration{};
  WorkerId worker{};
  WorkerBootId workerBootId{};
  std::string evidence;
  bool transient = false;
  bool recoverable = false;
  bool fatal = false;
  std::string causalMetadata;
  ResolutionState resolution = ResolutionState::OPEN;
  Timestamp resolvedAt{};

  friend bool operator==(const Fault&, const Fault&) = default;

  void write(wire::Writer& w) const {
    id.write(w);
    accelerator.write(w);
    ah::write(w, type);
    ah::write(w, faultClass);
    ah::write(w, severity);
    timestamp.write(w);
    observationGeneration.write(w);
    worker.write(w);
    workerBootId.write(w);
    w.string(evidence);
    w.u8(transient ? 1 : 0);
    w.u8(recoverable ? 1 : 0);
    w.u8(fatal ? 1 : 0);
    w.string(causalMetadata);
    ah::write(w, resolution);
    resolvedAt.write(w);
  }
  static bool read(wire::Reader& r, Fault& f) noexcept {
    f.id = FaultId::read(r);
    f.accelerator = AcceleratorId::read(r);
    if (!ah::read(r, f.type)) return false;
    if (!ah::read(r, f.faultClass)) return false;
    if (!ah::read(r, f.severity)) return false;
    f.timestamp = Timestamp::read(r);
    f.observationGeneration = ObservationGeneration::read(r);
    f.worker = WorkerId::read(r);
    f.workerBootId = WorkerBootId::read(r);
    f.evidence = r.string();
    f.transient = r.u8() != 0;
    f.recoverable = r.u8() != 0;
    f.fatal = r.u8() != 0;
    f.causalMetadata = r.string();
    if (!ah::read(r, f.resolution)) return false;
    f.resolvedAt = Timestamp::read(r);
    return r.ok();
  }
};

}  // namespace ah
