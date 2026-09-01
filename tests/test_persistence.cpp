// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#include "accelerator-health/Store.hpp"
#include "accelerator-health/Persistence.hpp"
#include "accelerator-health/Time.hpp"
#include "test_fw.hpp"
#include <cstdio>
#include <memory>
using namespace ah;

static std::shared_ptr<FakeClock> clk() { return std::make_shared<FakeClock>(0); }

static void populate(HealthStore& store, AcceleratorId id) {
  store.registerWorker(WorkerId{10}, WorkerBootId{100});
  AcceleratorObservation o;
  o.accelerator = id; o.node = NodeId{1}; o.worker = WorkerId{10}; o.workerBootId = WorkerBootId{100};
  o.observationGeneration = ObservationGeneration{1}; o.deviceGeneration = DeviceGeneration{1};
  o.timestamp = Timestamp{100};
  o.dimensions = { nominal(DimensionKind::ENUMERATION, EvidenceClass::MEASURED, Timestamp{100}),
                   nominal(DimensionKind::EXECUTION, EvidenceClass::MEASURED, Timestamp{100}),
                   nominal(DimensionKind::MEMORY_INTEGRITY, EvidenceClass::MEASURED, Timestamp{100}) };
  AuthorityEnvelope e;
  e.coordinatorEpoch = 1; e.workerId = WorkerId{10}; e.workerBootId = WorkerBootId{100};
  e.observationGeneration = ObservationGeneration{1}; e.deviceGeneration = DeviceGeneration{1};
  store.acceptObservation(o, e);
  ValidationRecord v;
  v.id = ValidationRunId{1}; v.accelerator = id; v.timestamp = Timestamp{200};
  v.profile = ValidationProfile::FULL; v.depth = ValidationDepth::FULL; v.passed = true; v.maxError = 0.0f;
  v.worker = WorkerId{10}; v.workerBootId = WorkerBootId{100}; v.validationGeneration = ValidationGeneration{1};
  store.recordValidation(v);
}

AH_TEST(roundtrip) {
  auto clock = clk();
  HealthStore s(clock, {}, 1);
  auto id = s.registerDevice(DeviceUuid{101}, NodeId{7});
  populate(s, id);
  auto bytes = s.encodeState();
  std::string err;
  HealthStore s2(clock, {}, 5);
  CHECK_MSG(HealthStore::decode(bytes, s2, &err), err.c_str());
  auto snap = s2.snapshot(id);
  // Health is preserved, but recovered dynamic evidence is never verifiably
  // CURRENT until a live worker refreshes it (requiresRevalidation marker).
  CHECK_EQ((int)snap.state, (int)HealthState::HEALTHY);
  // decode() is a faithful snapshot: readiness is preserved.
  CHECK_EQ((int)snap.readiness, (int)ReadinessState::READY);
  CHECK_EQ(s2.deviceIds().size(), 1u);
}

AH_TEST(save_load_file) {
  auto clock = clk();
  HealthStore s(clock, {}, 1);
  auto id = s.registerDevice(DeviceUuid{202}, NodeId{7});
  populate(s, id);
  std::string err;
  const std::string p = std::string(AH_BINARY_DIR) + "/ah_persist_test.bin";
  CHECK_MSG(s.saveToFile(p, &err), err.c_str());
  HealthStore s2(clock, {}, 5);
  CHECK_MSG(HealthStore::loadFromFile(p, s2, &err), err.c_str());
  auto snap = s2.snapshot(id);
  CHECK_EQ((int)snap.state, (int)HealthState::HEALTHY);
  // loadFromFile() is a recovery: dynamic evidence must not be falsely CURRENT.
  CHECK_EQ((int)snap.readiness, (int)ReadinessState::REVALIDATION_REQUIRED);
}

AH_TEST(corruption_rejected) {
  auto clock = clk();
  HealthStore s(clock, {}, 1);
  auto id = s.registerDevice(DeviceUuid{303}, NodeId{7});
  populate(s, id);
  auto bytes = s.encodeState();
  auto bad = bytes;
  bad[20] ^= 0xFF;
  std::string err;
  HealthStore s2(clock, {}, 5);
  CHECK_MSG(!HealthStore::decode(bad, s2, &err), "corruption must be rejected");
}

AH_TEST(truncation_rejected) {
  auto clock = clk();
  HealthStore s(clock, {}, 1);
  auto id = s.registerDevice(DeviceUuid{404}, NodeId{7});
  populate(s, id);
  auto bytes = s.encodeState();
  bytes.resize(bytes.size() - 3);
  std::string err;
  HealthStore s2(clock, {}, 5);
  CHECK_MSG(!HealthStore::decode(bytes, s2, &err), "truncation must be rejected");
}

AH_TEST(trailing_garbage_rejected) {
  auto clock = clk();
  HealthStore s(clock, {}, 1);
  auto id = s.registerDevice(DeviceUuid{505}, NodeId{7});
  populate(s, id);
  auto bytes = s.encodeState();
  bytes.push_back(0xAA);
  bytes.push_back(0xBB);
  std::string err;
  HealthStore s2(clock, {}, 5);
  CHECK_MSG(!HealthStore::decode(bytes, s2, &err), "trailing garbage must be rejected");
}

AH_TEST(unsupported_version_rejected) {
  auto clock = clk();
  HealthStore s(clock, {}, 1);
  auto id = s.registerDevice(DeviceUuid{606}, NodeId{7});
  populate(s, id);
  auto bytes = s.encodeState();
  bytes[4] = 99;  // version byte
  std::string err;
  HealthStore s2(clock, {}, 5);
  CHECK_MSG(!HealthStore::decode(bytes, s2, &err), "unsupported version must be rejected");
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  std::remove((std::string(AH_BINARY_DIR) + "/ah_persist_test.bin").c_str());
  return testfw::runAll("test_persistence");
}
