// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#include "accelerator-health/Policy.hpp"
#include "accelerator-health/Fault.hpp"
#include "test_fw.hpp"
using namespace ah;

AH_TEST(inherently_fatal_types) {
  CHECK(isInherentlyFatal(FaultType::DEVICE_LOST));
  CHECK(isInherentlyFatal(FaultType::MEMORY_INTEGRITY_FAILURE));
  CHECK(isInherentlyFatal(FaultType::IDENTITY_CHANGE));
  CHECK(isInherentlyFatal(FaultType::FATAL_RUNTIME_ERROR));
  CHECK(!isInherentlyFatal(FaultType::ALLOCATION_FAILURE));
  CHECK(!isInherentlyFatal(FaultType::TRANSFER_FAILURE));
  CHECK(!isInherentlyFatal(FaultType::STALE_EVIDENCE));
}

AH_TEST(policy_payload_roundtrip) {
  HealthPolicy p;
  p.id = PolicyId{7};
  p.requiredValidationDepth = ValidationDepth::FULL;
  p.degradeAfterFailures = 2;
  p.unhealthyAfterFailures = 3;
  p.quarantineAfterFailures = 4;
  p.recoveryValidationsRequired = 1;
  p.revalidationProfile = ValidationProfile::FULL;
  p.allowDegradedReadiness = true;
  p.fatalFaultTriggersQuarantine = true;
  wire::Writer w;
  p.write(w);
  wire::Reader r(w.data());
  HealthPolicy p2;
  CHECK(HealthPolicy::read(r, p2));
  CHECK(p == p2);
}

AH_TEST(severity_ordering) {
  CHECK(severity_rank(Severity::INFO) < severity_rank(Severity::WARNING));
  CHECK(severity_rank(Severity::WARNING) < severity_rank(Severity::DEGRADED));
  CHECK(severity_rank(Severity::DEGRADED) < severity_rank(Severity::CRITICAL));
  CHECK(severity_rank(Severity::CRITICAL) < severity_rank(Severity::FATAL));
}

int main(int argc, char** argv) { (void)argc; (void)argv; return testfw::runAll("test_fault"); }
