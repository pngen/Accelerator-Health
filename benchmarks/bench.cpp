// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
// Micro-benchmarks for selected completed operations. Each reports ops/second and
// a per-op cost; workloads are bounded and clearly labelled.
#include "accelerator-health/Protocol.hpp"
#include "accelerator-health/Store.hpp"
#include "accelerator-health/Time.hpp"
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
using namespace ah;

static std::uint64_t nowNs(){
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
template<class F> double usPerOp(int n, F f){ auto a=nowNs(); for(int i=0;i<n;++i) f(i); auto b=nowNs(); return static_cast<double>(b-a)/static_cast<double>(n)/1000.0; }

int main(){
  auto c=std::make_shared<FakeClock>(0);
  HealthStore s(c,{},1);
  auto id=s.registerDevice(DeviceUuid{1},NodeId{1});
  s.registerWorker(WorkerId{10},WorkerBootId{100});
  AcceleratorObservation o; o.accelerator=id; o.worker=WorkerId{10}; o.workerBootId=WorkerBootId{100};
  o.observationGeneration=ObservationGeneration{1}; o.deviceGeneration=DeviceGeneration{1}; o.timestamp=Timestamp{1};
  o.dimensions={nominal(DimensionKind::ENUMERATION,EvidenceClass::MEASURED,Timestamp{1}),nominal(DimensionKind::EXECUTION,EvidenceClass::MEASURED,Timestamp{1})};
  AuthorityEnvelope e; e.coordinatorEpoch=1; e.workerId=WorkerId{10}; e.workerBootId=WorkerBootId{100};
  e.observationGeneration=ObservationGeneration{1}; e.deviceGeneration=DeviceGeneration{1};
  s.acceptObservation(o,e);
  ValidationRecord v; v.id=ValidationRunId{1};v.accelerator=id;v.timestamp=Timestamp{2};v.profile=ValidationProfile::FULL;v.depth=ValidationDepth::FULL;v.passed=true;v.worker=WorkerId{10};v.workerBootId=WorkerBootId{100}; s.recordValidation(v);

  const int N=100000;
  std::printf("benchmark: accelerator-health (fake backend), Debug\n");
  std::printf("  observation ingestion     : %6.3f us/op\n", usPerOp(N,[&](int i){ AcceleratorObservation oo=o; oo.observationGeneration=ObservationGeneration{static_cast<std::uint64_t>(i+1)}; s.acceptObservation(oo,e);}));
  std::printf("  health assessment (snapshot): %6.3f us/op\n", usPerOp(N,[&](int){ (void)s.snapshot(id);}));
  std::printf("  indexed lookup (health)    : %6.3f us/op\n", usPerOp(N,[&](int){ (void)s.byHealthState(HealthState::HEALTHY);}));
  Fault f; f.id=FaultId{1};f.accelerator=id;f.type=FaultType::TRANSFER_FAILURE;f.severity=Severity::DEGRADED;f.faultClass=FaultClass::TRANSIENT;f.transient=true;f.recoverable=true;
  std::printf("  fault recording            : %6.3f us/op\n", usPerOp(10000,[&](int i){ Fault ff=f; ff.id=FaultId{static_cast<std::uint64_t>(1000+i+1)}; s.recordFault(ff);}));
  std::printf("  protocol encode/decode     : %6.3f us/op\n", usPerOp(N,[&](int){ wire::Writer w; w.u32(42); w.string(std::string("x")); auto bytes=proto::encodeFrame(proto::MessageType::OBSERVATION,w.data()); proto::Frame out; std::size_t con=0; std::string err; proto::decodeFrame(std::span<const std::uint8_t>(bytes),out,con,&err);}));
  std::printf("  persistence encode         : %6.3f us/op\n", usPerOp(1000,[&](int){ (void)s.encodeState();}));
  return 0;
}
