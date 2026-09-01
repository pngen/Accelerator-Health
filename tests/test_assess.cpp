// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#include "accelerator-health/Assessment.hpp"
#include "accelerator-health/Dimension.hpp"
#include "test_fw.hpp"
using namespace ah;

static AssessmentInput baseIn() {
  AssessmentInput in;
  in.accelerator = AcceleratorId{1};
  in.uuid = DeviceUuid{1};
  in.deviceGeneration = DeviceGeneration{1};
  in.currentState = HealthState::INITIALIZING;
  in.now = Timestamp{1000};
  in.lastObservation = Timestamp{500};
  in.highestValidationDepth = ValidationDepth::FULL;
  in.policy = HealthPolicy{};
  return in;
}

AH_TEST(healthy_nominal) {
  AssessmentEngine e;
  auto in = baseIn();
  in.dimensions[DimensionKind::ENUMERATION] = nominal(DimensionKind::ENUMERATION, EvidenceClass::MEASURED, Timestamp{500});
  in.dimensions[DimensionKind::EXECUTION] = nominal(DimensionKind::EXECUTION, EvidenceClass::MEASURED, Timestamp{500});
  auto a = e.assess(in);
  CHECK_EQ((int)a.state, (int)HealthState::HEALTHY);
  CHECK_EQ((int)a.readiness, (int)ReadinessState::READY);
  CHECK(a.executionReady);
  CHECK(!a.digest.empty());
}

AH_TEST(failed_dimension_unhealthy) {
  AssessmentEngine e;
  auto in = baseIn();
  in.dimensions[DimensionKind::EXECUTION] = failed(DimensionKind::EXECUTION, EvidenceClass::MEASURED, Timestamp{500}, Severity::CRITICAL, "kernel timeout");
  auto a = e.assess(in);
  CHECK_EQ((int)a.state, (int)HealthState::UNHEALTHY);
}

AH_TEST(stale_reduces_confidence) {
  AssessmentEngine e;
  auto in = baseIn();
  in.lastObservation = Timestamp{100};  // 900ns old, default expire=300s -> CURRENT still
  // Force expired via lastObservation far past
  in.lastObservation = Timestamp{0};
  in.now = Timestamp{400 * kSec};
  in.dimensions[DimensionKind::ENUMERATION] = nominal(DimensionKind::ENUMERATION, EvidenceClass::MEASURED, Timestamp{0});
  auto a = e.assess(in);
  CHECK(a.confidence < 1.0f);
  CHECK(a.state != HealthState::HEALTHY);
}

AH_TEST(explain_is_stable) {
  AssessmentEngine e;
  auto in = baseIn();
  in.dimensions[DimensionKind::ENUMERATION] = nominal(DimensionKind::ENUMERATION, EvidenceClass::MEASURED, Timestamp{500});
  auto a1 = e.assess(in);
  auto a2 = e.assess(in);
  CHECK_EQ(a1.digest, a2.digest);
  auto d1 = e.explain(a1);
  auto d2 = e.explain(a2);
  CHECK_EQ(d1.text, d2.text);
}

int main(int argc, char** argv) { (void)argc; (void)argv; return testfw::runAll("test_assess"); }
