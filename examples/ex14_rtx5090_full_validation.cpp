#include "accelerator-health/CudaBackend.hpp"
#include <cstdio>
using namespace ah;
int main(){
  CudaBackend be;
  auto devs = be.enumerate();
  if(devs.empty()){ std::printf("no CUDA device\n"); return 1; }
  auto d0=devs[0];
  std::printf("device=%s name=%s cc=%d.%d total=%zu free=%zu\n",d0.id.string().c_str(),d0.name.c_str(),d0.computeCapabilityMajor,d0.computeCapabilityMinor,d0.totalMemory,d0.freeMemory);
  auto v=be.runValidation(d0.id,ValidationProfile::FULL);
  std::printf("validation passed=%d depth=%s maxError=%f freeBefore=%zu freeAfter=%zu\n",v.passed?1:0,to_string(v.depth),v.maxError,v.freeBefore,v.freeAfter);
  return v.passed?0:1;
}
