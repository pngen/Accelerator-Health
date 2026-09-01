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
#include "accelerator-health/Time.hpp"

namespace ah {

// Freshness thresholds expressed as durations. The evaluator is deterministic
// given a clock and these thresholds.
struct FreshnessThresholds {
  DurationNanos current = 5 * kSec;   // <= current => CURRENT
  DurationNanos stale = 30 * kSec;    // <= stale   => AGING
  DurationNanos expire = 300 * kSec;  // <= expire  => STALE
  // beyond expire => EXPIRED

  friend bool operator==(const FreshnessThresholds&, const FreshnessThresholds&) = default;
};

class FreshnessEvaluator {
 public:
  explicit FreshnessEvaluator(FreshnessThresholds t = {}) : thresholds_(t) {}
  FreshnessStatus evaluate(Timestamp observed, Timestamp now) const noexcept {
    if (observed > now) return FreshnessStatus::CURRENT;
    const auto age = now.nanos - observed.nanos;
    if (age <= thresholds_.current) return FreshnessStatus::CURRENT;
    if (age <= thresholds_.stale) return FreshnessStatus::AGING;
    if (age <= thresholds_.expire) return FreshnessStatus::STALE;
    return FreshnessStatus::EXPIRED;
  }
  const FreshnessThresholds& thresholds() const noexcept { return thresholds_; }

 private:
  FreshnessThresholds thresholds_;
};

}  // namespace ah
