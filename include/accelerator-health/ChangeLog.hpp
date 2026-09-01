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

#include <string>
#include <vector>

namespace ah {

// A health change record. History is never rewritten: each change is appended.
struct HealthChange {
  Timestamp timestamp{};
  AcceleratorId accelerator{};
  HealthState from = HealthState::UNKNOWN;
  HealthState to = HealthState::UNKNOWN;
  ReadinessState readinessFrom = ReadinessState::UNKNOWN;
  ReadinessState readinessTo = ReadinessState::UNKNOWN;
  std::string label;  // e.g. "health", "evidence", "validation", "policy", "generation"
  std::string detail;
  HealthGeneration healthGeneration{};
  PolicyGeneration policyGeneration{};

  friend bool operator==(const HealthChange&, const HealthChange&) = default;

  void write(wire::Writer& w) const {
    timestamp.write(w);
    accelerator.write(w);
    ah::write(w, from);
    ah::write(w, to);
    ah::write(w, readinessFrom);
    ah::write(w, readinessTo);
    w.string(label);
    w.string(detail);
    healthGeneration.write(w);
    policyGeneration.write(w);
  }
  static bool read(wire::Reader& r, HealthChange& hc) noexcept {
    hc.timestamp = Timestamp::read(r);
    hc.accelerator = AcceleratorId::read(r);
    if (!ah::read(r, hc.from)) return false;
    if (!ah::read(r, hc.to)) return false;
    if (!ah::read(r, hc.readinessFrom)) return false;
    if (!ah::read(r, hc.readinessTo)) return false;
    hc.label = r.string();
    hc.detail = r.string();
    hc.healthGeneration = HealthGeneration::read(r);
    hc.policyGeneration = PolicyGeneration::read(r);
    return r.ok();
  }
};

// Append-only change log for one device.
class ChangeLog {
 public:
  void append(HealthChange c) { changes_.push_back(std::move(c)); }
  std::size_t size() const { return changes_.size(); }
  bool empty() const { return changes_.empty(); }
  const std::vector<HealthChange>& all() const { return changes_; }
  std::vector<HealthChange> recent(std::size_t limit) const {
    if (limit >= changes_.size()) return changes_;
    return std::vector<HealthChange>(changes_.end() - static_cast<std::ptrdiff_t>(limit), changes_.end());
  }
  void write(wire::Writer& w) const {
    w.u32(static_cast<std::uint32_t>(changes_.size()));
    for (auto& c : changes_) c.write(w);
  }
  static bool read(wire::Reader& r, ChangeLog& log) noexcept {
    const auto n = r.u32();
    if (r.fail() || n > util::kMaxCount) { r.set_fail(); return false; }
    log.changes_.clear();
    log.changes_.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i) {
      HealthChange hc;
      if (!HealthChange::read(r, hc)) return false;
      log.changes_.push_back(std::move(hc));
    }
    return r.ok();
  }

 private:
  std::vector<HealthChange> changes_;
};

}  // namespace ah
