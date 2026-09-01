// Basic health assessment.
#include "accelerator-health/Store.hpp"
#include "accelerator-health/Time.hpp"
#include <cstdio>
#include <memory>
using namespace ah;
int main(){
  auto clk = std::make_shared<FakeClock>(0);
  HealthStore store(clk, HealthPolicy{}, 1);
  auto id = store.registerDevice(DeviceUuid{1}, NodeId{1});
  store.registerWorker(WorkerId{10}, WorkerBootId{100});
  AcceleratorObservation o;
  o.accelerator=id; o.node=NodeId{1}; o.worker=WorkerId{10}; o.workerBootId=WorkerBootId{100};
  o.observationGeneration=ObservationGeneration{1}; o.deviceGeneration=DeviceGeneration{1}; o.timestamp=Timestamp{10};
  o.dimensions={nominal(DimensionKind::ENUMERATION,EvidenceClass::MEASURED,Timestamp{10}),
                nominal(DimensionKind::EXECUTION,EvidenceClass::MEASURED,Timestamp{10})};
  AuthorityEnvelope e; e.coordinatorEpoch=1; e.workerId=WorkerId{10}; e.workerBootId=WorkerBootId{100};
  e.observationGeneration=ObservationGeneration{1}; e.deviceGeneration=DeviceGeneration{1};
  store.acceptObservation(o,e);
  auto s = store.snapshot(id);
  std::printf("state=%s readiness=%s\n", to_string(s.state), to_string(s.readiness));
  return 0;
}
