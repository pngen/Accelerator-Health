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

#include "accelerator-health/Assessment.hpp"
#include "accelerator-health/Fault.hpp"

#include <map>
#include <vector>

namespace ah {

// Append-only fault history for a single accelerator. Prior failures are never
// erased after recovery; they only change resolution state. Counts are computed
// from canonical state (never cached in a way that can drift).
class FaultHistory {
 public:
  void record(const Fault& f);
  void record(Fault&& f);
  // Resolve an open fault. Returns false if the fault id was not open (prevents
  // double resolution).
  bool resolve(FaultId id, Timestamp at);

  std::size_t size() const { return faults_.size(); }
  bool empty() const { return faults_.empty(); }
  const std::vector<Fault>& all() const { return faults_; }

  std::vector<Fault> openFaults() const;
  std::vector<Fault> faultsOfType(FaultType t) const;
  std::vector<Fault> recent(std::size_t limit) const;
  std::uint32_t countOfType(FaultType t) const;
  std::uint32_t consecutiveOfType(FaultType t) const;
  Timestamp firstOfType(FaultType t) const;
  Timestamp latestOfType(FaultType t) const;
  bool isFaultOpen(FaultId id) const;

  // Overall stats for the assessment engine.
  DeviceFaultStats stats() const;

  void write(wire::Writer& w) const;
  static bool read(wire::Reader& r, FaultHistory& h);

 private:
  // faults_ ordered by insertion (id) for deterministic iteration.
  std::vector<Fault> faults_;
};

}  // namespace ah
