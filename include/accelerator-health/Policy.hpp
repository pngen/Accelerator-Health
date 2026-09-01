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
#include "accelerator-health/Freshness.hpp"
#include "accelerator-health/StrongIdentity.hpp"

#include <cstdint>

namespace ah {

// Typed health policy. A policy change produces a PolicyGeneration rollover; old
// assessments produced under a stale policy must not silently remain current.
struct HealthPolicy {
  PolicyId id{};
  ValidationDepth requiredValidationDepth = ValidationDepth::FULL;
  FreshnessThresholds freshness{};

  std::uint32_t degradeAfterFailures = 2;
  std::uint32_t unhealthyAfterFailures = 3;
  std::uint32_t quarantineAfterFailures = 4;
  std::uint32_t recoveryValidationsRequired = 1;
  ValidationProfile revalidationProfile = ValidationProfile::FULL;

  bool allowDegradedReadiness = true;
  bool fatalFaultTriggersQuarantine = true;
  bool requireEnumeratedForHealth = true;

  friend bool operator==(const HealthPolicy&, const HealthPolicy&) = default;

  void write(wire::Writer& w) const {
    id.write(w);
    ah::write(w, requiredValidationDepth);
    w.u64(static_cast<std::uint64_t>(freshness.current));
    w.u64(static_cast<std::uint64_t>(freshness.stale));
    w.u64(static_cast<std::uint64_t>(freshness.expire));
    w.u32(degradeAfterFailures);
    w.u32(unhealthyAfterFailures);
    w.u32(quarantineAfterFailures);
    w.u32(recoveryValidationsRequired);
    ah::write(w, revalidationProfile);
    w.u8(allowDegradedReadiness ? 1 : 0);
    w.u8(fatalFaultTriggersQuarantine ? 1 : 0);
    w.u8(requireEnumeratedForHealth ? 1 : 0);
  }
  static bool read(wire::Reader& r, HealthPolicy& p) noexcept {
    p.id = PolicyId::read(r);
    if (!ah::read(r, p.requiredValidationDepth)) return false;
    p.freshness.current = static_cast<DurationNanos>(r.u64());
    p.freshness.stale = static_cast<DurationNanos>(r.u64());
    p.freshness.expire = static_cast<DurationNanos>(r.u64());
    p.degradeAfterFailures = r.u32();
    p.unhealthyAfterFailures = r.u32();
    p.quarantineAfterFailures = r.u32();
    p.recoveryValidationsRequired = r.u32();
    if (!ah::read(r, p.revalidationProfile)) return false;
    p.allowDegradedReadiness = r.u8() != 0;
    p.fatalFaultTriggersQuarantine = r.u8() != 0;
    p.requireEnumeratedForHealth = r.u8() != 0;
    return r.ok();
  }
};

bool isInherentlyFatal(FaultType t) noexcept;

}  // namespace ah
