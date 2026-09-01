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
#include "accelerator-health/Enums.hpp"
#include "accelerator-health/Policy.hpp"
#include "accelerator-health/Readiness.hpp"

#include <map>
#include <string>
#include <vector>

namespace ah {

// Deterministic, inspectable health assessment. No opaque scoring: every field is
// derived from explicit rules and recorded evidence.
struct HealthAssessment {
  HealthAssessmentId id{};
  AcceleratorId accelerator{};
  DeviceUuid uuid{};
  HealthState state = HealthState::UNKNOWN;
  ReadinessState readiness = ReadinessState::UNKNOWN;
  ValidationDepth validationDepth = ValidationDepth::NONE;
  float confidence = 0.0f;
  ActionRequired action = ActionRequired::UNKNOWN;
  bool executionReady = false;
  bool quarantined = false;
  QuarantineId quarantineId{};
  bool inRecovery = false;
  Timestamp timestamp{};
  HealthGeneration healthGeneration{};
  PolicyGeneration policyGeneration{};
  DeviceGeneration deviceGeneration{};
  std::vector<std::string> reasons;
  std::string digest;

  friend bool operator==(const HealthAssessment&, const HealthAssessment&) = default;
};

// Explanation of an assessment: deterministically-ordered text, JSON and digest.
struct Diagnosis {
  std::string text;
  std::string json;
  std::string digest;
};

// Per-device fault counters used by the engine.
struct DeviceFaultStats {
  std::uint32_t total = 0;
  std::uint32_t criticalFatalOpen = 0;  // open CRITICAL/FATAL faults
  std::uint32_t consecutiveLatest = 0;  // consecutive failures of the most recent type
  bool hasOpenFatal = false;
  bool hasOpenCritical = false;
};

// All inputs the assessment engine needs for one device.
struct AssessmentInput {
  AcceleratorId accelerator{};
  DeviceUuid uuid{};
  DeviceGeneration deviceGeneration{};
  HealthState currentState = HealthState::UNKNOWN;
  bool quarantined = false;
  bool inRecovery = false;
  Timestamp now{};
  std::map<DimensionKind, HealthDimension> dimensions;  // latest per kind, stable key order
  DeviceFaultStats faultStats{};
  ValidationDepth highestValidationDepth = ValidationDepth::NONE;
  Timestamp lastObservation{};
  HealthPolicy policy{};
  bool deviceGenerationChanged = false;
  bool policyGenerationChanged = false;
};

class AssessmentEngine {
 public:
  HealthAssessment assess(const AssessmentInput& in) const;
  // Deterministic explanation paired with a fresh assessment.
  Diagnosis explain(const HealthAssessment& a) const;
};

}  // namespace ah
