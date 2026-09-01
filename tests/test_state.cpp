// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#include "accelerator-health/StateMachine.hpp"
#include "test_fw.hpp"
using namespace ah;

AH_TEST(legal_recovery_sequence) {
  StateMachine m(HealthState::UNHEALTHY);
  bool threw = false;
  try { m.transitionTo(HealthState::HEALTHY); } catch (const InvalidTransition&) { threw = true; }
  CHECK_MSG(threw, "UNHEALTHY->HEALTHY must be illegal");
  // go through recovery
  m.transitionTo(HealthState::DRAINING);
  m.transitionTo(HealthState::DRAINED);
  m.transitionTo(HealthState::RECOVERING);
  m.transitionTo(HealthState::REVALIDATING);
  m.transitionTo(HealthState::HEALTHY);
  CHECK_EQ((int)m.state(), (int)HealthState::HEALTHY);
}

AH_TEST(no_bypass_healthy) {
  CHECK(!canTransition(HealthState::QUARANTINED, HealthState::HEALTHY));
  CHECK(!canTransition(HealthState::DEGRADED, HealthState::HEALTHY));
  CHECK(!canTransition(HealthState::UNHEALTHY, HealthState::HEALTHY));
  CHECK(canTransition(HealthState::REVALIDATING, HealthState::HEALTHY));
  CHECK(canTransition(HealthState::INITIALIZING, HealthState::HEALTHY));
  CHECK(canTransition(HealthState::OFFLINE, HealthState::HEALTHY));
}

AH_TEST(quarantine_recovery_legal) {
  CHECK(canTransition(HealthState::QUARANTINED, HealthState::RECOVERING));
  CHECK(!canTransition(HealthState::QUARANTINED, HealthState::HEALTHY));
  CHECK(canTransition(HealthState::RECOVERING, HealthState::REVALIDATING));
}

AH_TEST(invalid_transition_throws) {
  StateMachine m(HealthState::HEALTHY);
  bool threw = false;
  try { m.transitionTo(HealthState::RECOVERING); } catch (const InvalidTransition&) { threw = true; }
  CHECK_MSG(threw, "HEALTHY->RECOVERING illegal");
}

int main(int argc, char** argv) { (void)argc; (void)argv; return testfw::runAll("test_state"); }
