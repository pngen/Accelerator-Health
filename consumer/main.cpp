// Downstream find_package consumer.
#include "accelerator-health/Store.hpp"
#include "accelerator-health/Time.hpp"
#include <cstdio>
#include <memory>
using namespace ah;
int main(){
  auto c = std::make_shared<FakeClock>(0);
  HealthStore s(c, {}, 1);
  auto id = s.registerDevice(DeviceUuid{1}, NodeId{1});
  s.registerWorker(WorkerId{10}, WorkerBootId{100});
  AcceleratorObservation o; o.accelerator=id; o.worker=WorkerId{10}; o.workerBootId=WorkerBootId{100};
  o.observationGeneration=ObservationGeneration{1}; o.deviceGeneration=DeviceGeneration{1}; o.timestamp=Timestamp{10};
  o.dimensions={nominal(DimensionKind::ENUMERATION,EvidenceClass::MEASURED,Timestamp{10})};
  AuthorityEnvelope e; e.coordinatorEpoch=1; e.workerId=WorkerId{10}; e.workerBootId=WorkerBootId{100};
  e.observationGeneration=ObservationGeneration{1}; e.deviceGeneration=DeviceGeneration{1};
  s.acceptObservation(o,e);
  std::printf("consumer state=%s\n", to_string(s.snapshot(id).state));
  return 0;
}
