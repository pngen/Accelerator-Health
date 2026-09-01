// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#include "accelerator-health/Store.hpp"
#include "accelerator-health/Time.hpp"
#include "test_fw.hpp"
#include <memory>
#include <thread>
#include <vector>
using namespace ah;

AH_TEST(concurrent_access) {
  auto clock = std::make_shared<FakeClock>(0);
  HealthStore store(clock, {}, 1);
  auto id = store.registerDevice(DeviceUuid{77}, NodeId{1});
  store.registerWorker(WorkerId{10}, WorkerBootId{100});
  // healthy setup
  AcceleratorObservation o;
  o.accelerator = id; o.worker = WorkerId{10}; o.workerBootId = WorkerBootId{100};
  o.observationGeneration = ObservationGeneration{1}; o.deviceGeneration = DeviceGeneration{1};
  o.timestamp = Timestamp{10};
  o.dimensions = { nominal(DimensionKind::ENUMERATION, EvidenceClass::MEASURED, Timestamp{10}) };
  AuthorityEnvelope e; e.coordinatorEpoch = 1; e.workerId = WorkerId{10}; e.workerBootId = WorkerBootId{100};
  e.observationGeneration = ObservationGeneration{1}; e.deviceGeneration = DeviceGeneration{1};
  store.acceptObservation(o, e);

  std::vector<std::thread> threads;
  for (int t = 0; t < 8; ++t) {
    threads.emplace_back([&store, t, id] {
      for (int j = 0; j < 200; ++j) {
        if ((t + j) % 3 == 0) {
          Fault f;
          f.id = FaultId{static_cast<std::uint64_t>(t * 1000 + j + 1)};
          f.accelerator = id; f.type = FaultType::ALLOCATION_FAILURE;
          f.severity = Severity::WARNING; f.faultClass = FaultClass::TRANSIENT;
          f.transient = true; f.recoverable = true;
          f.evidence = "synthetic";
          store.recordFault(f);
        } else {
          (void)store.snapshot(id);
          (void)store.accounting();
        }
      }
    });
  }
  for (auto& th : threads) th.join();
  auto snap = store.snapshot(id);
  CHECK(snap.state == HealthState::HEALTHY || snap.state == HealthState::DEGRADED ||
        snap.state == HealthState::UNHEALTHY || snap.state == HealthState::QUARANTINED);
  auto acct = store.accounting();
  CHECK_EQ(acct.tracked, 1u);
  CHECK(acct.unresolvedFaults > 0u);
}

int main(int argc, char** argv) { (void)argc; (void)argv; return testfw::runAll("test_concurrency"); }
