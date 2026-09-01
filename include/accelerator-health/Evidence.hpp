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

#include "accelerator-health/Dimension.hpp"

#include <string>
#include <vector>

namespace ah {

// A single evidence record: one observation of one dimension with full
// provenance (source class, timestamp, generations, worker incarnation).
struct Evidence {
  ObservationId id{};
  AcceleratorId accelerator{};
  DimensionKind dimension = DimensionKind::UNKNOWN;
  DimensionState state = DimensionState::UNKNOWN;
  EvidenceClass evidenceClass = EvidenceClass::UNKNOWN;
  Timestamp timestamp{};
  ObservationGeneration observationGeneration{};
  WorkerId workerId{};
  WorkerBootId workerBootId{};
  DeviceGeneration deviceGeneration{};
  float confidence = 0.0f;
  Severity severity = Severity::INFO;
  std::string reason;
  double value = 0.0;
  std::string units;

  friend bool operator==(const Evidence&, const Evidence&) = default;

  void write(wire::Writer& w) const {
    id.write(w);
    accelerator.write(w);
    ah::write(w, dimension);
    ah::write(w, state);
    ah::write(w, evidenceClass);
    timestamp.write(w);
    observationGeneration.write(w);
    workerId.write(w);
    workerBootId.write(w);
    deviceGeneration.write(w);
    w.f32(confidence);
    ah::write(w, severity);
    w.string(reason);
    w.f64(value);
    w.string(units);
  }
  static bool read(wire::Reader& r, Evidence& e) noexcept {
    e.id = ObservationId::read(r);
    e.accelerator = AcceleratorId::read(r);
    if (!ah::read(r, e.dimension)) return false;
    if (!ah::read(r, e.state)) return false;
    if (!ah::read(r, e.evidenceClass)) return false;
    e.timestamp = Timestamp::read(r);
    e.observationGeneration = ObservationGeneration::read(r);
    e.workerId = WorkerId::read(r);
    e.workerBootId = WorkerBootId::read(r);
    e.deviceGeneration = DeviceGeneration::read(r);
    e.confidence = r.f32();
    if (!ah::read(r, e.severity)) return false;
    e.reason = r.string();
    e.value = r.f64();
    e.units = r.string();
    return r.ok();
  }
};

// A grouped observation for one accelerator carrying many dimensions.
struct AcceleratorObservation {
  AcceleratorId accelerator{};
  NodeId node{};
  WorkerId worker{};
  WorkerBootId workerBootId{};
  ObservationGeneration observationGeneration{};
  DeviceGeneration deviceGeneration{};
  Timestamp timestamp{};
  std::vector<HealthDimension> dimensions;

  friend bool operator==(const AcceleratorObservation&, const AcceleratorObservation&) = default;

  void write(wire::Writer& w) const {
    accelerator.write(w);
    node.write(w);
    worker.write(w);
    workerBootId.write(w);
    observationGeneration.write(w);
    deviceGeneration.write(w);
    timestamp.write(w);
    w.u32(static_cast<std::uint32_t>(dimensions.size()));
    for (auto& d : dimensions) d.write(w);
  }
  static bool read(wire::Reader& r, AcceleratorObservation& o) noexcept {
    o.accelerator = AcceleratorId::read(r);
    o.node = NodeId::read(r);
    o.worker = WorkerId::read(r);
    o.workerBootId = WorkerBootId::read(r);
    o.observationGeneration = ObservationGeneration::read(r);
    o.deviceGeneration = DeviceGeneration::read(r);
    o.timestamp = Timestamp::read(r);
    const auto n = r.u32();
    if (r.fail() || n > util::kMaxCount) { r.set_fail(); return false; }
    o.dimensions.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i) {
      HealthDimension d;
      if (!HealthDimension::read(r, d)) return false;
      o.dimensions.push_back(std::move(d));
    }
    return r.ok();
  }
};

}  // namespace ah
