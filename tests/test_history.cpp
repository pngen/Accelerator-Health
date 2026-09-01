// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#include "accelerator-health/History.hpp"
#include "test_fw.hpp"
#include <stdexcept>
using namespace ah;

static Fault mk(FaultId id, FaultType t, Timestamp ts, Severity s) {
  Fault f;
  f.id = id; f.accelerator = AcceleratorId{1}; f.type = t; f.severity = s; f.timestamp = ts;
  f.faultClass = FaultClass::TRANSIENT; f.transient = true; f.recoverable = true; f.fatal = false;
  f.resolution = ResolutionState::OPEN; f.evidence = "synthetic";
  return f;
}

AH_TEST(append_only_and_resolve) {
  FaultHistory h;
  h.record(mk(FaultId{1}, FaultType::TRANSFER_FAILURE, Timestamp{1}, Severity::DEGRADED));
  h.record(mk(FaultId{2}, FaultType::TRANSFER_FAILURE, Timestamp{2}, Severity::DEGRADED));
  CHECK_EQ(h.size(), 2u);
  CHECK(h.isFaultOpen(FaultId{1}));
  CHECK(h.resolve(FaultId{1}, Timestamp{10}));
  CHECK(!h.resolve(FaultId{1}, Timestamp{20}));  // no double resolution
  CHECK(!h.isFaultOpen(FaultId{1}));
}

AH_TEST(duplicate_fault_id_rejected) {
  FaultHistory h;
  h.record(mk(FaultId{9}, FaultType::DRIVER_FAILURE, Timestamp{1}, Severity::WARNING));
  bool threw = false;
  try { h.record(mk(FaultId{9}, FaultType::DRIVER_FAILURE, Timestamp{2}, Severity::WARNING)); }
  catch (const std::invalid_argument&) { threw = true; }
  CHECK_MSG(threw, "duplicate fault id must be rejected");
}

AH_TEST(stats) {
  FaultHistory h;
  h.record(mk(FaultId{1}, FaultType::TRANSFER_FAILURE, Timestamp{1}, Severity::DEGRADED));
  h.record(mk(FaultId{2}, FaultType::TRANSFER_FAILURE, Timestamp{2}, Severity::DEGRADED));
  h.record(mk(FaultId{3}, FaultType::TRANSFER_FAILURE, Timestamp{3}, Severity::CRITICAL));
  auto s = h.stats();
  CHECK_EQ(s.total, 3u);
  CHECK_EQ(s.consecutiveLatest, 3u);
  CHECK(s.hasOpenCritical);
  CHECK(!s.hasOpenFatal);
  h.resolve(FaultId{1}, Timestamp{10});
  h.resolve(FaultId{2}, Timestamp{11});
  h.resolve(FaultId{3}, Timestamp{12});
  s = h.stats();
  CHECK_EQ(s.total, 3u);
  CHECK_EQ(s.criticalFatalOpen, 0u);
}

AH_TEST(first_and_latest) {
  FaultHistory h;
  h.record(mk(FaultId{1}, FaultType::KERNEL_LAUNCH_FAILURE, Timestamp{5}, Severity::WARNING));
  h.record(mk(FaultId{2}, FaultType::KERNEL_LAUNCH_FAILURE, Timestamp{50}, Severity::WARNING));
  CHECK_EQ(h.firstOfType(FaultType::KERNEL_LAUNCH_FAILURE).nanos, 5);
  CHECK_EQ(h.latestOfType(FaultType::KERNEL_LAUNCH_FAILURE).nanos, 50);
}

int main(int argc, char** argv) { (void)argc; (void)argv; return testfw::runAll("test_history"); }
