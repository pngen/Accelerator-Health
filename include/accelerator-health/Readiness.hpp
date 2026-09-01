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
#include "accelerator-health/Policy.hpp"
#include "accelerator-health/StrongIdentity.hpp"

#include <string>
#include <vector>

namespace ah {

// Inputs the readiness evaluator needs. Readiness is deliberately separated from
// health: a HEALTHY device may still be NOT_READY (stale evidence, incomplete
// validation, changed policy or device generation).
struct ReadinessInput {
  HealthState health = HealthState::UNKNOWN;
  FreshnessStatus freshness = FreshnessStatus::EXPIRED;
  ValidationDepth validationDepth = ValidationDepth::NONE;
  ValidationDepth requiredValidationDepth = ValidationDepth::FULL;
  bool quarantined = false;
  bool inRecovery = false;
  bool deviceGenerationChanged = false;
  bool policyGenerationChanged = false;
  bool requiresRevalidation = false;
  bool allowDegradedReadiness = true;
  bool runtimeCompatibilityChanged = false;
};

struct ReadinessResult {
  ReadinessState state = ReadinessState::UNKNOWN;
  bool executionReady = false;
  std::vector<std::string> reasons;
};

class ReadinessEvaluator {
 public:
  ReadinessResult evaluate(const ReadinessInput& in) const;
};

}  // namespace ah
