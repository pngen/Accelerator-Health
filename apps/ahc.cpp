// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
// Accelerator Health CLI.
#include "accelerator-health/Assessment.hpp"
#include "accelerator-health/Backend.hpp"
#include "accelerator-health/FakeBackend.hpp"
#include "accelerator-health/Persistence.hpp"
#include "accelerator-health/Store.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#ifdef AH_HAS_CUDA
#include "accelerator-health/CudaBackend.hpp"
#endif

using namespace ah;

namespace {
struct Ctx {
  std::shared_ptr<Clock> clock = std::make_shared<SystemClock>();
  HealthStore store{clock, HealthPolicy{}, 1};
  std::shared_ptr<Backend> backend = std::make_shared<FakeBackend>();
  std::vector<DeviceIdentity> devices;
  int deviceIndex = 1;  // start assigning AcceleratorIds
};

std::string help() {
  return "ahc <command> [args]\n"
         "  list\n"
         "  inspect <id|uuid>\n"
         "  health <id|uuid>\n"
         "  readiness <id|uuid>\n"
         "  faults <id|uuid>\n"
         "  history <id|uuid>\n"
         "  validate <id|uuid> [profile]\n"
         "  quarantine <id|uuid> [reason]\n"
         "  clear-quarantine <id|uuid>\n"
         "  begin-recovery <id|uuid>\n"
         "  revalidate <id|uuid>\n"
         "  explain <id|uuid>\n"
         "  changes <id|uuid>\n"
         "  snapshot\n"
         "  save <file>\n"
         "  recover <file>\n"
         "  benchmark [n]\n";
}

AcceleratorId resolveId(Ctx& ctx, const std::string& s) {
  std::size_t pos = 0;
  try { std::uint64_t v = std::stoull(s, &pos); if (pos == s.size()) return AcceleratorId{v}; } catch (...) {}
  for (auto& d : ctx.devices) if (d.uuid.string() == s) return d.id;
  for (const auto& id : ctx.store.deviceIds()) if (id.string() == s) return id;
  return AcceleratorId{};
}

void registerBackendDevices(Ctx& ctx) {
  ctx.devices = ctx.backend->enumerate();
  if (ctx.devices.empty()) {
    auto fb = std::dynamic_pointer_cast<FakeBackend>(ctx.backend);
    if (fb) {
      DeviceIdentity d;
      d.id = AcceleratorId{1};
      d.uuid = DeviceUuid{1};
      d.name = "synthetic-accelerator";
      d.computeCapabilityMajor = 12;
      d.computeCapabilityMinor = 0;
      d.totalMemory = 1u << 30;
      d.freeMemory = 1u << 30;
      d.supported = true;
      fb->addDevice(d);
      ctx.devices = ctx.backend->enumerate();
    }
  }
  for (auto& d : ctx.devices) {
    auto id = ctx.store.registerDevice(d.uuid, NodeId{1});
    d.id = id;
  }
}

void printAssessment(const HealthAssessment& a) {
  std::printf("  state=%s readiness=%s confidence=%.3f validation=%s action=%s execReady=%d digest=%s\n",
              to_string(a.state), to_string(a.readiness), a.confidence, to_string(a.validationDepth),
              to_string(a.action), a.executionReady ? 1 : 0, a.digest.c_str());
  for (auto& r : a.reasons) std::printf("    - %s\n", r.c_str());
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fputs(help().c_str(), stderr); return 1; }
  std::string cmd = argv[1];
#ifdef AH_HAS_CUDA
  bool useCuda = false;
  for (int i = 2; i < argc; ++i) if (std::string(argv[i]) == "--cuda") useCuda = true;
#endif
  Ctx ctx;
#ifdef AH_HAS_CUDA
  if (useCuda) ctx.backend = std::make_shared<CudaBackend>();
#endif
  registerBackendDevices(ctx);

  if (cmd == "list") {
    for (auto& d : ctx.devices) {
      auto id = ctx.store.registerDevice(d.uuid, NodeId{1});
      auto snap = ctx.store.snapshot(id);
      std::printf("device=%s uuid=%s name=%s cc=%d.%d state=%s readiness=%s\n",
                  id.string().c_str(), d.uuid.string().c_str(), d.name.c_str(), d.computeCapabilityMajor,
                  d.computeCapabilityMinor, to_string(snap.state), to_string(snap.readiness));
    }
    return 0;
  }
  if (cmd == "help") { std::fputs(help().c_str(), stdout); return 0; }
  if (cmd == "snapshot") {
    for (auto& id : ctx.store.deviceIds()) {
      auto snap = ctx.store.snapshot(id);
      std::printf("device=%s state=%s readiness=%s validation=%s faults=%u digest=%s\n",
                  id.string().c_str(), to_string(snap.state), to_string(snap.readiness),
                  to_string(snap.highestValidation), snap.faultCount, snap.latestAssessment.digest.c_str());
    }
    return 0;
  }
  if (cmd == "save" && argc >= 3) {
    std::string err;
    if (ctx.store.saveToFile(argv[2], &err)) { std::printf("saved %s\n", argv[2]); return 0; }
    std::printf("ERROR %s\n", err.c_str()); return 1;
  }
  if (cmd == "recover" && argc >= 3) {
    std::string err;
    if (HealthStore::loadFromFile(argv[2], ctx.store, &err)) { std::printf("recovered %s\n", argv[2]); return 0; }
    std::printf("ERROR %s\n", err.c_str()); return 1;
  }
  if (cmd == "benchmark") {
    int n = argc >= 3 ? std::atoi(argv[2]) : 10000;
    auto t0 = ctx.clock->now();
    for (int i = 0; i < n; ++i) {
      (void)ctx.store.snapshot(ctx.store.deviceIds().empty() ? AcceleratorId{1} : ctx.store.deviceIds()[0]);
    }
    auto t1 = ctx.clock->now();
    std::printf("benchmark reassess-equivalent snapshot queries: %d ops in %lld ns => %.1f ns/op\n",
                n, (long long)(t1.nanos - t0.nanos), (double)(t1.nanos - t0.nanos) / (double)n);
    return 0;
  }

  if (argc < 3) { std::fputs(help().c_str(), stderr); return 1; }
  auto id = resolveId(ctx, argv[2]);
  if (id.is_null()) { std::printf("unknown device %s\n", argv[2]); return 1; }

  if (cmd == "inspect") {
    auto s = ctx.store.snapshot(id);
    std::printf("device=%s uuid=%s node=%s state=%s readiness=%s validation=%s changed=%d\n",
                id.string().c_str(), s.uuid.string().c_str(), s.node.string().c_str(), to_string(s.state), to_string(s.readiness),
                to_string(s.highestValidation), s.deviceGenerationChanged ? 1 : 0);
    return 0;
  }
  if (cmd == "health" || cmd == "readiness" || cmd == "explain") {
    auto a = ctx.store.assessmentOf(id);
    printAssessment(a);
    if (cmd == "explain") { Diagnosis d = AssessmentEngine{}.explain(a); std::printf("%s\n", d.text.c_str()); }
    return 0;
  }
  if (cmd == "faults") {
    for (auto& f : ctx.store.faultsOf(id)) {
      std::printf("fault=%s type=%s severity=%s class=%s open=%d ts=%lld\n", f.id.string().c_str(),
                  to_string(f.type), to_string(f.severity), to_string(f.faultClass),
                  f.resolution == ResolutionState::OPEN ? 1 : 0, (long long)f.timestamp.nanos);
    }
    return 0;
  }
  if (cmd == "history" || cmd == "changes") {
    for (auto& c : ctx.store.changesOf(id)) {
      std::printf("%lld %s -> %s [%s] %s\n", (long long)c.timestamp.nanos, to_string(c.from),
                  to_string(c.to), c.label.c_str(), c.detail.c_str());
    }
    return 0;
  }
  if (cmd == "validate") {
    ValidationProfile prof = ValidationProfile::FULL;
    if (argc >= 4) { auto p = parse_validation_profile(argv[3]); if (p) prof = *p; }
    auto result = ctx.backend->runValidation(id, prof);
    ValidationRecord v;
    v.id = ValidationRunId{1}; v.accelerator = id; v.timestamp = ctx.clock->now();
    v.profile = prof; v.depth = result.depth; v.passed = result.passed; v.maxError = result.maxError;
    v.worker = WorkerId{0}; v.workerBootId = WorkerBootId{0};
    ctx.store.recordValidation(v);
    std::printf("validate device=%s passed=%d depth=%s maxError=%f\n", id.string().c_str(), result.passed ? 1 : 0,
                to_string(result.depth), result.maxError);
    return result.passed ? 0 : 1;
  }
  if (cmd == "quarantine") {
    ctx.store.beginQuarantine(id, QuarantineAuthority::OPERATOR, argc >= 4 ? argv[3] : "operator", "operator");
    std::printf("quarantined %s\n", id.string().c_str());
    return 0;
  }
  if (cmd == "clear-quarantine") { ctx.store.clearQuarantine(id); std::printf("cleared %s\n", id.string().c_str()); return 0; }
  if (cmd == "begin-recovery") { ctx.store.beginRecovery(id); std::printf("recovery %s\n", id.string().c_str()); return 0; }
  if (cmd == "revalidate") {
    ValidationRecord v;
    v.id = ValidationRunId{2}; v.accelerator = id; v.timestamp = ctx.clock->now();
    v.profile = ValidationProfile::FULL; v.depth = ValidationDepth::FULL; v.passed = true; v.maxError = 0.0f;
    ctx.store.revalidate(id, v);
    auto s = ctx.store.snapshot(id);
    std::printf("revalidate device=%s state=%s readiness=%s\n", id.string().c_str(), to_string(s.state), to_string(s.readiness));
    return 0;
  }

  std::fputs(help().c_str(), stderr);
  return 1;
}