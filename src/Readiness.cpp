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
#include "accelerator-health/Readiness.hpp"

namespace ah {

ReadinessResult ReadinessEvaluator::evaluate(const ReadinessInput& in) const {
  ReadinessResult r;
  if (in.quarantined) {
    r.state = ReadinessState::QUARANTINED;
    r.executionReady = false;
    r.reasons.push_back("quarantine active");
    return r;
  }
  // Stale/expired evidence undermines any positive health claim. A readable
  // device cannot be trusted without fresh evidence, so force revalidation.
  if ((in.health == HealthState::HEALTHY || in.health == HealthState::DEGRADED) &&
      (in.freshness == FreshnessStatus::STALE || in.freshness == FreshnessStatus::EXPIRED)) {
    r.state = ReadinessState::REVALIDATION_REQUIRED;
    r.executionReady = false;
    r.reasons.push_back("evidence stale/expired; revalidation required");
    return r;
  }
  switch (in.health) {
    case HealthState::FAILED:
      r.state = ReadinessState::DRAIN_REQUIRED;
      r.executionReady = false;
      r.reasons.push_back("device failed; drain required");
      break;
    case HealthState::UNHEALTHY:
      r.state = ReadinessState::DRAIN_REQUIRED;
      r.executionReady = false;
      r.reasons.push_back("device unhealthy; drain required");
      break;
    case HealthState::DEGRADED:
      if (in.allowDegradedReadiness) {
        r.state = ReadinessState::READY_DEGRADED;
        r.executionReady = true;
        r.reasons.push_back("degraded but policy permits degraded readiness");
      } else {
        r.state = ReadinessState::NOT_READY;
        r.executionReady = false;
        r.reasons.push_back("degraded and policy forbids degraded readiness");
      }
      break;
    case HealthState::HEALTHY:
      if (in.freshness == FreshnessStatus::STALE || in.freshness == FreshnessStatus::EXPIRED) {
        r.state = ReadinessState::REVALIDATION_REQUIRED;
        r.executionReady = false;
        r.reasons.push_back("healthy but evidence is stale/expired");
      } else if (in.validationDepth < in.requiredValidationDepth) {
        r.state = ReadinessState::REVALIDATION_REQUIRED;
        r.executionReady = false;
        r.reasons.push_back("validation depth below required");
      } else if (in.deviceGenerationChanged || in.policyGenerationChanged || in.runtimeCompatibilityChanged) {
        r.state = ReadinessState::REVALIDATION_REQUIRED;
        r.executionReady = false;
        r.reasons.push_back("generation or compatibility changed");
      } else if (in.requiresRevalidation) {
        r.state = ReadinessState::REVALIDATION_REQUIRED;
        r.executionReady = false;
        r.reasons.push_back("revalidation required after recovery");
      } else if (in.freshness == FreshnessStatus::AGING) {
        r.state = ReadinessState::READY_DEGRADED;
        r.executionReady = true;
        r.reasons.push_back("evidence aging; degraded readiness");
      } else {
        r.state = ReadinessState::READY;
        r.executionReady = true;
        r.reasons.push_back("healthy and evidence current");
      }
      break;
    case HealthState::UNKNOWN:
    case HealthState::INITIALIZING:
      r.state = ReadinessState::UNKNOWN;
      r.executionReady = false;
      r.reasons.push_back("health unknown/initializing");
      break;
    case HealthState::DRAINING:
    case HealthState::DRAINED:
    case HealthState::RECOVERING:
    case HealthState::REVALIDATING:
      r.state = ReadinessState::REVALIDATION_REQUIRED;
      r.executionReady = false;
      r.reasons.push_back("device in recovery/revalidation");
      break;
    case HealthState::QUARANTINED:
      r.state = ReadinessState::QUARANTINED;
      r.executionReady = false;
      r.reasons.push_back("quarantine active");
      break;
    case HealthState::OFFLINE:
    case HealthState::LOST:
      r.state = ReadinessState::NOT_READY;
      r.executionReady = false;
      r.reasons.push_back("device offline/lost");
      break;
  }
  return r;
}

}  // namespace ah
