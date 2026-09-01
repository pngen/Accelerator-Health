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

#include "accelerator-health/Enums.hpp"
#include "accelerator-health/StrongIdentity.hpp"
#include "accelerator-health/Time.hpp"

#include <cmath>
#include <string>

namespace ah {

// A single health dimension: current state, evidence source, timestamp,
// observation generation, confidence, severity, reason and optional measurement.
struct HealthDimension {
  DimensionKind kind = DimensionKind::UNKNOWN;
  DimensionState state = DimensionState::UNKNOWN;
  EvidenceClass evidence = EvidenceClass::UNKNOWN;
  Timestamp timestamp{};
  ObservationGeneration observationGeneration{};
  DeviceGeneration deviceGeneration{};
  float confidence = 0.0f;
  Severity severity = Severity::INFO;
  std::string reason;
  double value = 0.0;
  std::string units;

  friend bool operator==(const HealthDimension& a, const HealthDimension& b) noexcept {
    return a.kind == b.kind && a.state == b.state && a.evidence == b.evidence &&
           a.timestamp == b.timestamp && a.observationGeneration == b.observationGeneration &&
           a.deviceGeneration == b.deviceGeneration && a.confidence == b.confidence &&
           a.severity == b.severity && a.reason == b.reason && a.value == b.value && a.units == b.units;
  }

  bool isNominal() const noexcept { return state == DimensionState::NOMINAL; }
  bool hasMeasurement() const noexcept { return !units.empty(); }
  bool isFinite() const noexcept { return std::isfinite(static_cast<double>(confidence)); }

  void write(wire::Writer& w) const {
    ah::write(w, kind);
    ah::write(w, state);
    ah::write(w, evidence);
    timestamp.write(w);
    observationGeneration.write(w);
    deviceGeneration.write(w);
    w.f32(confidence);
    ah::write(w, severity);
    w.string(reason);
    w.f64(value);
    w.string(units);
  }
  static bool read(wire::Reader& r, HealthDimension& d) noexcept {
    if (!ah::read(r, d.kind)) return false;
    if (!ah::read(r, d.state)) return false;
    if (!ah::read(r, d.evidence)) return false;
    d.timestamp = Timestamp::read(r);
    d.observationGeneration = ObservationGeneration::read(r);
    d.deviceGeneration = DeviceGeneration::read(r);
    d.confidence = r.f32();
    if (!ah::read(r, d.severity)) return false;
    d.reason = r.string();
    d.value = r.f64();
    d.units = r.string();
    return r.ok();
  }
};

// Convenience constructors for nominal / degraded / failed dimensions.
inline HealthDimension nominal(DimensionKind kind, EvidenceClass ev, Timestamp t,
                               Severity sev = Severity::INFO, std::string reason = {}) {
  HealthDimension d;
  d.kind = kind; d.state = DimensionState::NOMINAL; d.evidence = ev; d.timestamp = t;
  d.severity = sev; d.reason = std::move(reason); d.confidence = 1.0f;
  return d;
}
inline HealthDimension degraded(DimensionKind kind, EvidenceClass ev, Timestamp t,
                                Severity sev, std::string reason) {
  HealthDimension d;
  d.kind = kind; d.state = DimensionState::DEGRADED; d.evidence = ev; d.timestamp = t;
  d.severity = sev; d.reason = std::move(reason); d.confidence = 0.6f;
  return d;
}
inline HealthDimension failed(DimensionKind kind, EvidenceClass ev, Timestamp t,
                              Severity sev, std::string reason) {
  HealthDimension d;
  d.kind = kind; d.state = DimensionState::FAILED; d.evidence = ev; d.timestamp = t;
  d.severity = sev; d.reason = std::move(reason); d.confidence = 0.2f;
  return d;
}

}  // namespace ah
