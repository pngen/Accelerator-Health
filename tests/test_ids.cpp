// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#include "accelerator-health/StrongIdentity.hpp"
#include "test_fw.hpp"
#include <unordered_set>
using namespace ah;

AH_TEST(types_are_disjoint) {
  AcceleratorId a{1};
  WorkerId w{1};
  CHECK_EQ(a.get(), 1u);
  CHECK_EQ(w.get(), 1u);
  CHECK(a != AcceleratorId{2});
  CHECK(a == AcceleratorId{1});
}

AH_TEST(generation_rollover) {
  HealthGeneration h{1};
  ValidationGeneration v{1};
  ++h;
  ++v;
  CHECK_EQ(h.get(), 2u);
  CHECK_EQ(v.get(), 2u);
  // Independent: bumping health does not bump validation.
  ++h;
  CHECK_EQ(h.get(), 3u);
  CHECK_EQ(v.get(), 2u);
}

AH_TEST(id_wire_roundtrip) {
  wire::Writer w;
  AuthorityEnvelope e;
  e.coordinatorEpoch = 3;
  e.workerId = WorkerId{5};
  e.workerBootId = WorkerBootId{6};
  e.observationGeneration = ObservationGeneration{7};
  e.healthGeneration = HealthGeneration{8};
  e.validationGeneration = ValidationGeneration{9};
  e.deviceGeneration = DeviceGeneration{10};
  e.write(w);
  wire::Reader r(w.data());
  auto e2 = AuthorityEnvelope::read(r);
  CHECK_EQ(r.fail(), false);
  CHECK(e == e2);
}

AH_TEST(hashable) {
  std::unordered_set<AcceleratorId> s;
  s.insert(AcceleratorId{1});
  s.insert(AcceleratorId{2});
  s.insert(AcceleratorId{1});
  CHECK_EQ(s.size(), 2u);
}

int main(int argc, char** argv) { (void)argc; (void)argv; return testfw::runAll("test_ids"); }
