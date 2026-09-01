// Acceleration Health example.
#include "accelerator-health/Store.hpp"
#include "accelerator-health/Assessment.hpp"
#include "accelerator-health/Time.hpp"
#include <cstdio>
#include <memory>
using namespace ah;
static std::shared_ptr<FakeClock> clk(){ return std::make_shared<FakeClock>(0); }
static AcceleratorObservation nominalObs(AcceleratorId id, Timestamp ts){
  AcceleratorObservation o; o.accelerator=id; o.worker=WorkerId{10}; o.workerBootId=WorkerBootId{100};
  o.observationGeneration=ObservationGeneration{1}; o.deviceGeneration=DeviceGeneration{1}; o.timestamp=ts;
  o.dimensions={nominal(DimensionKind::ENUMERATION,EvidenceClass::MEASURED,ts),nominal(DimensionKind::EXECUTION,EvidenceClass::MEASURED,ts),nominal(DimensionKind::MEMORY_INTEGRITY,EvidenceClass::MEASURED,ts)};
  return o;
}
static AuthorityEnvelope env(){ AuthorityEnvelope e; e.coordinatorEpoch=1; e.workerId=WorkerId{10}; e.workerBootId=WorkerBootId{100}; e.observationGeneration=ObservationGeneration{1}; e.deviceGeneration=DeviceGeneration{1}; return e; }
void setup(HealthStore& s, AcceleratorId id){ s.registerWorker(WorkerId{10},WorkerBootId{100}); s.acceptObservation(nominalObs(id,Timestamp{10}), env()); ValidationRecord v; v.id=ValidationRunId{1}; v.accelerator=id; v.timestamp=Timestamp{20}; v.profile=ValidationProfile::FULL; v.depth=ValidationDepth::FULL; v.passed=true; v.worker=WorkerId{10}; v.workerBootId=WorkerBootId{100}; s.recordValidation(v); }
int main(){
  auto c=clk(); HealthStore s(c,{},1); auto id=s.registerDevice(DeviceUuid{8},NodeId{1}); setup(s,id);
  std::printf("fresh: readiness=%s\n",to_string(s.snapshot(id).readiness));
  c->advance(400*kSec); s.reassess(id);
  std::printf("stale: state=%s readiness=%s\n",to_string(s.snapshot(id).state),to_string(s.snapshot(id).readiness));
  return 0;
}