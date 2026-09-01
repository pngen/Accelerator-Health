// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#include "accelerator-health/FakeBackend.hpp"
#include "accelerator-health/Util.hpp"
#include <algorithm>
#include <cmath>

namespace ah {

FakeBackend::FakeBackend(bool cudaAvailable) : cudaAvailable_(cudaAvailable) {}

std::vector<DeviceIdentity> FakeBackend::enumerate() {
  std::vector<DeviceIdentity> out;
  for (const auto& [id, d] : devices_) { (void)id; out.push_back(d); }
  return out;
}
bool FakeBackend::queryIdentity(AcceleratorId id, DeviceIdentity& out) {
  auto it = devices_.find(id);
  if (it == devices_.end()) return false;
  out = it->second;
  return true;
}
MemoryInfo FakeBackend::memory(AcceleratorId) { return mem_; }
ThermalInfo FakeBackend::temperature(AcceleratorId) { return temp_; }
PowerInfo FakeBackend::power(AcceleratorId) { return power_; }
ErrorInfo FakeBackend::errors(AcceleratorId) { return errors_; }
LinkInfo FakeBackend::link(AcceleratorId) { return link_; }

ValidationResult FakeBackend::runValidation(AcceleratorId id, ValidationProfile profile) {
  (void)id;
  ValidationResult r;
  r.profile = profile;
  if (forceValidationFailure_) {
    r.passed = false;
    r.error = failureReason_;
    r.depth = ValidationDepth::NONE;
    r.steps.push_back("synthetic failure forced");
    return r;
  }
  const std::uint64_t base = mem_.free != 0 ? mem_.free : (1ull << 30);
  r.freeBefore = base;
  r.steps = {"step 1: device enumeration", "step 2: identity confirmation",
             "step 3: CUDA initialization", "step 4: bounded allocation",
             "step 5: host-to-device transfer", "step 6: real CUDA kernel execution",
             "step 7: synchronization", "step 8: device-to-host transfer",
             "step 9: CPU-reference comparison", "step 10: resource cleanup",
             "step 11: memory recovery check"};
  if (stepsLimit_ > 0 && static_cast<int>(r.steps.size()) > stepsLimit_) r.steps.resize(stepsLimit_);

  util::SplitMix64 rng(0x1234567890ABCDEFull);
  double maxErr = 0.0;
  const int n = 1024;
  for (int i = 0; i < n; ++i) {
    const float in = static_cast<float>(rng.next() & 0xFFFF) / 65535.0f;
    const float ref = in * 2.0f;
    float dev = ref;
    if (forceNumericalMismatch_) dev = ref + forcedMaxError_;
    const float e = std::fabs(dev - ref);
    if (static_cast<double>(e) > maxErr) maxErr = static_cast<double>(e);
  }
  r.maxError = static_cast<float>(maxErr);
  r.freeAfter = base;
  if (forceNumericalMismatch_ || maxErr > 1e-6) {
    r.passed = false;
    r.error = "numerical mismatch, max error " + std::to_string(r.maxError);
  } else {
    r.passed = true;
    r.depth = (profile == ValidationProfile::FULL) ? ValidationDepth::FULL
              : (profile == ValidationProfile::NUMERICAL) ? ValidationDepth::NUMERICAL
              : (profile == ValidationProfile::EXECUTION) ? ValidationDepth::EXECUTION
              : (profile == ValidationProfile::TRANSFER) ? ValidationDepth::TRANSFER
              : (profile == ValidationProfile::MEMORY) ? ValidationDepth::ALLOCATION
              : ValidationDepth::INITIALIZATION;
  }
  return r;
}

}  // namespace ah
