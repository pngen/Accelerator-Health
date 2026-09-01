// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#include "accelerator-health/Store.hpp"
#include "accelerator-health/Persistence.hpp"
#include "accelerator-health/Time.hpp"
#include "test_fw.hpp"

#include <memory>
#include <map>

using namespace ah;

static std::shared_ptr<FakeClock> clk() { return std::make_shared<FakeClock>(0); }

static AcceleratorObservation nominalObs(AcceleratorId id, DeviceUuid uuid, Timestamp ts,
                                         DeviceGeneration gen, ObservationGeneration og) {
  AcceleratorObservation o;
  (void)uuid;
  o.accelerator = id;
  o.node = NodeId{1};
  o.worker = WorkerId{10};
  o.workerBootId = WorkerBootId{100};
  o.observationGeneration = og;
  o.deviceGeneration = gen;
  o.timestamp = ts;
  o.dimensions = {
    nominal(DimensionKind::ENUMERATION, EvidenceClass::MEASURED, ts),
    nominal(DimensionKind::RUNTIME_INIT, EvidenceClass::MEASURED, ts),
    nominal(DimensionKind::ALLOCATION, EvidenceClass::MEASURED, ts),
    nominal(DimensionKind::TRANSFER, EvidenceClass::MEASURED, ts),
    nominal(DimensionKind::EXECUTION, EvidenceClass::MEASURED, ts),
    nominal(DimensionKind::SYNCHRONIZATION, EvidenceClass::MEASURED, ts),
    nominal(DimensionKind::MEMORY_INTEGRITY, EvidenceClass::MEASURED, ts),
  };
  return o;
}

static AuthorityEnvelope env(std::uint64_t epoch, WorkerId w, WorkerBootId b,
                            ObservationGeneration og, DeviceGeneration gen) {
  AuthorityEnvelope e;
  e.coordinatorEpoch = epoch;
  e.workerId = w;
  e.workerBootId = b;
  e.observationGeneration = og;
  e.deviceGeneration = gen;
  e.healthGeneration = HealthGeneration{1};
  e.validationGeneration = ValidationGeneration{1};
  return e;
}

static ValidationRecord fullVal(AcceleratorId id, Timestamp ts, WorkerId w, WorkerBootId b) {
  ValidationRecord v;
  v.id = ValidationRunId{1};
  v.accelerator = id;
  v.timestamp = ts;
  v.profile = ValidationProfile::FULL;
  v.depth = ValidationDepth::FULL;
  v.passed = true;
  v.maxError = 0.0f;
  v.worker = w;
  v.workerBootId = b;
  v.validationGeneration = ValidationGeneration{1};
  v.deviceGeneration = DeviceGeneration{1};
  return v;
}

static Fault fault(AcceleratorId id, FaultId fid, FaultType type, Timestamp ts, Severity sev) {
  Fault f;
  f.id = fid;
  f.accelerator = id;
  f.type = type;
  f.faultClass = FaultClass::TRANSIENT;
  f.severity = sev;
  f.timestamp = ts;
  f.observationGeneration = ObservationGeneration{1};
  f.worker = WorkerId{10};
  f.workerBootId = WorkerBootId{100};
  f.transient = true;
  f.recoverable = true;
  f.fatal = false;
  f.evidence = "synthetic";
  f.resolution = ResolutionState::OPEN;
  return f;
}

AH_TEST(basic_healthy_readiness) {
  auto clock = clk();
  HealthStore store(clock, {}, 1);
  auto id = store.registerDevice(DeviceUuid{100}, NodeId{7});
  store.registerWorker(WorkerId{10}, WorkerBootId{100});
  auto o = nominalObs(id, DeviceUuid{100}, Timestamp{100}, DeviceGeneration{1}, ObservationGeneration{1});
  auto res = store.acceptObservation(o, env(1, WorkerId{10}, WorkerBootId{100}, ObservationGeneration{1}, DeviceGeneration{1}));
  CHECK_MSG(res == "accepted", res.c_str());
  // No validation yet: state HEALTHY but readiness REVALIDATION_REQUIRED.
  auto snap = store.snapshot(id);
  CHECK_EQ((int)snap.state, (int)HealthState::HEALTHY);
  CHECK_EQ((int)snap.readiness, (int)ReadinessState::REVALIDATION_REQUIRED);
  // Now full validation.
  store.recordValidation(fullVal(id, Timestamp{200}, WorkerId{10}, WorkerBootId{100}));
  snap = store.snapshot(id);
  CHECK_EQ((int)snap.state, (int)HealthState::HEALTHY);
  CHECK_EQ((int)snap.readiness, (int)ReadinessState::READY);
  CHECK(snap.latestAssessment.executionReady);
}

AH_TEST(failure_hysteresis) {
  auto clock = clk();
  HealthStore store(clock, {}, 1);
  auto id = store.registerDevice(DeviceUuid{200}, NodeId{7});
  store.registerWorker(WorkerId{10}, WorkerBootId{100});
  auto o = nominalObs(id, DeviceUuid{200}, Timestamp{100}, DeviceGeneration{1}, ObservationGeneration{1});
  store.acceptObservation(o, env(1, WorkerId{10}, WorkerBootId{100}, ObservationGeneration{1}, DeviceGeneration{1}));
  store.recordValidation(fullVal(id, Timestamp{200}, WorkerId{10}, WorkerBootId{100}));
  CHECK_EQ((int)store.snapshot(id).state, (int)HealthState::HEALTHY);
  // 1 transient fault: still HEALTHY.
  store.recordFault(fault(id, FaultId{1}, FaultType::TRANSFER_FAILURE, Timestamp{300}, Severity::DEGRADED));
  CHECK_EQ((int)store.snapshot(id).state, (int)HealthState::HEALTHY);
  // 2 faults: DEGRADED.
  store.recordFault(fault(id, FaultId{2}, FaultType::TRANSFER_FAILURE, Timestamp{400}, Severity::DEGRADED));
  CHECK_EQ((int)store.snapshot(id).state, (int)HealthState::DEGRADED);
  // 3 faults: UNHEALTHY.
  store.recordFault(fault(id, FaultId{3}, FaultType::TRANSFER_FAILURE, Timestamp{500}, Severity::DEGRADED));
  CHECK_EQ((int)store.snapshot(id).state, (int)HealthState::UNHEALTHY);
}

AH_TEST(quarantine_recovery) {
  auto clock = clk();
  HealthStore store(clock, {}, 1);
  auto id = store.registerDevice(DeviceUuid{300}, NodeId{7});
  store.registerWorker(WorkerId{10}, WorkerBootId{100});
  auto o = nominalObs(id, DeviceUuid{300}, Timestamp{100}, DeviceGeneration{1}, ObservationGeneration{1});
  store.acceptObservation(o, env(1, WorkerId{10}, WorkerBootId{100}, ObservationGeneration{1}, DeviceGeneration{1}));
  store.recordValidation(fullVal(id, Timestamp{200}, WorkerId{10}, WorkerBootId{100}));
  store.beginQuarantine(id, QuarantineAuthority::FAULT, "fatal fault", "DEVICE_LOST");
  auto snap = store.snapshot(id);
  CHECK_EQ((int)snap.state, (int)HealthState::QUARANTINED);
  CHECK_EQ((int)snap.readiness, (int)ReadinessState::QUARANTINED);
  CHECK(snap.quarantine && snap.quarantine->active);
  // clear quarantine
  store.clearQuarantine(id);
  snap = store.snapshot(id);
  CHECK(!(snap.quarantine && snap.quarantine->active));
  // begin recovery
  store.beginRecovery(id);
  snap = store.snapshot(id);
  CHECK_EQ((int)snap.state, (int)HealthState::RECOVERING);
  // revalidate FULL -> HEALTHY
  store.revalidate(id, fullVal(id, Timestamp{900}, WorkerId{10}, WorkerBootId{100}));
  snap = store.snapshot(id);
  CHECK_EQ((int)snap.state, (int)HealthState::HEALTHY);
  CHECK(snap.latestAssessment.executionReady);
}

AH_TEST(stale_evidence) {
  auto clock = clk();
  HealthStore store(clock, {}, 1);
  auto id = store.registerDevice(DeviceUuid{400}, NodeId{7});
  store.registerWorker(WorkerId{10}, WorkerBootId{100});
  auto o = nominalObs(id, DeviceUuid{400}, Timestamp{100}, DeviceGeneration{1}, ObservationGeneration{1});
  store.acceptObservation(o, env(1, WorkerId{10}, WorkerBootId{100}, ObservationGeneration{1}, DeviceGeneration{1}));
  store.recordValidation(fullVal(id, Timestamp{200}, WorkerId{10}, WorkerBootId{100}));
  CHECK_EQ((int)store.snapshot(id).readiness, (int)ReadinessState::READY);
  // advance clock way past expire (300s)
  clock->advance(400 * kSec);
  store.reassess(id);
  auto snap = store.snapshot(id);
  CHECK_EQ((int)snap.state, (int)HealthState::DEGRADED);
  CHECK_EQ((int)snap.readiness, (int)ReadinessState::REVALIDATION_REQUIRED);
}

AH_TEST(fencing_stale_reports) {
  auto clock = clk();
  HealthStore store(clock, {}, 1);
  auto id = store.registerDevice(DeviceUuid{500}, NodeId{7});
  store.registerWorker(WorkerId{10}, WorkerBootId{100});
  // stale coordinator epoch
  auto o = nominalObs(id, DeviceUuid{500}, Timestamp{100}, DeviceGeneration{1}, ObservationGeneration{1});
  auto r1 = store.acceptObservation(o, env(0, WorkerId{10}, WorkerBootId{100}, ObservationGeneration{1}, DeviceGeneration{1}));
  CHECK_MSG(r1.rfind("rejected", 0) == 0, r1.c_str());
  // stale worker boot id
  auto r2 = store.acceptObservation(o, env(1, WorkerId{10}, WorkerBootId{999}, ObservationGeneration{1}, DeviceGeneration{1}));
  CHECK_MSG(r2.rfind("rejected", 0) == 0, r2.c_str());
  // unregistered worker
  auto r3 = store.acceptObservation(o, env(1, WorkerId{99}, WorkerBootId{100}, ObservationGeneration{1}, DeviceGeneration{1}));
  CHECK_MSG(r3.rfind("rejected", 0) == 0, r3.c_str());
  // stale observation generation (replay same generation)
  store.acceptObservation(o, env(1, WorkerId{10}, WorkerBootId{100}, ObservationGeneration{1}, DeviceGeneration{1}));
  auto r4 = store.acceptObservation(o, env(1, WorkerId{10}, WorkerBootId{100}, ObservationGeneration{1}, DeviceGeneration{1}));
  CHECK_MSG(r4.rfind("rejected", 0) == 0, r4.c_str());
  // stale device generation
  auto r5 = store.acceptObservation(o, env(1, WorkerId{10}, WorkerBootId{100}, ObservationGeneration{2}, DeviceGeneration{0}));
  CHECK_MSG(r5.rfind("rejected", 0) == 0, r5.c_str());
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  return testfw::runAll("test_core");
}
