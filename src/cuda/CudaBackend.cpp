// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#include "accelerator-health/CudaBackend.hpp"
#include <cuda_runtime.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

extern "C" int ah_cuda_launch_mult2(const float* in, float* out, int n);

namespace ah {

namespace {
const char* errText(cudaError_t e) { return cudaGetErrorString(e); }
bool checkCuda(cudaError_t e, ValidationResult& out, const char* step) {
  if (e != cudaSuccess) {
    out.error = std::string(step) + ": " + errText(e);
    return false;
  }
  out.steps.push_back(step);
  return true;
}
DeviceIdentity makeIdentity(int ordinal, int ccMajor, int ccMinor, const char* name,
                            std::uint64_t total, std::uint64_t free) {
  DeviceIdentity d;
  d.id = AcceleratorId{static_cast<std::uint64_t>(ordinal + 1)};
  d.uuid = DeviceUuid{static_cast<std::uint64_t>(0x50905090ull + static_cast<std::uint64_t>(ordinal))};
  d.generation = DeviceGeneration{1};
  d.name = name;
  d.computeCapabilityMajor = ccMajor;
  d.computeCapabilityMinor = ccMinor;
  d.totalMemory = total;
  d.freeMemory = free;
  d.supported = true;
  return d;
}
}  // namespace

void CudaBackend::setForceValidationFailure(bool on, std::string reason) {
  forceValidationFailure_ = on;
  failureReason_ = std::move(reason);
}
void CudaBackend::setForceNumericalMismatch(bool on, float maxError) {
  forceNumericalMismatch_ = on;
  forcedMaxError_ = maxError;
}
void CudaBackend::setMemoryRecoveryTolerance(std::uint64_t bytes) { memoryTolerance_ = bytes; }

bool CudaBackend::isCudaRuntimeAvailable() {
  int count = 0;
  cudaError_t e = cudaGetDeviceCount(&count);
  return e == cudaSuccess && count >= 1;
}

RuntimeInfo CudaBackend::runtime() {
  RuntimeInfo r;
  int runtimeVersion = 0;
  int driverVersion = 0;
  if (cudaRuntimeGetVersion(&runtimeVersion) == cudaSuccess) {
    r.cudaAvailable = true;
    r.version = std::to_string(runtimeVersion / 1000) + "." + std::to_string((runtimeVersion % 100) / 10);
  }
  if (cudaDriverGetVersion(&driverVersion) == cudaSuccess) {
    r.driver = std::to_string(driverVersion / 1000) + "." + std::to_string((driverVersion % 100) / 10);
  }
  return r;
}

std::vector<DeviceIdentity> CudaBackend::enumerate() {
  std::vector<DeviceIdentity> out;
  int count = 0;
  if (cudaGetDeviceCount(&count) != cudaSuccess) return out;
  for (int i = 0; i < count; ++i) {
    cudaDeviceProp prop;
    if (cudaGetDeviceProperties(&prop, i) != cudaSuccess) continue;
    std::size_t free = 0, total = 0;
    cudaMemGetInfo(&free, &total);
    out.push_back(makeIdentity(i, prop.major, prop.minor, prop.name, total, free));
  }
  return out;
}

bool CudaBackend::queryIdentity(AcceleratorId id, DeviceIdentity& out) {
  const int ordinal = static_cast<int>(id.get()) - 1;
  cudaDeviceProp prop;
  if (cudaGetDeviceProperties(&prop, ordinal) != cudaSuccess) return false;
  std::size_t free = 0, total = 0;
  cudaMemGetInfo(&free, &total);
  out = makeIdentity(ordinal, prop.major, prop.minor, prop.name, total, free);
  return true;
}

MemoryInfo CudaBackend::memory(AcceleratorId id) {
  (void)id;
  MemoryInfo m;
  std::size_t free = 0, total = 0;
  if (cudaMemGetInfo(&free, &total) == cudaSuccess) {
    m.available = true;
    m.free = free;
    m.total = total;
    m.used = total - free;
  }
  return m;
}
ThermalInfo CudaBackend::temperature(AcceleratorId) { return {}; }  // unsupported without NVML; not healthy evidence
PowerInfo CudaBackend::power(AcceleratorId) { return {}; }
ErrorInfo CudaBackend::errors(AcceleratorId) { return {}; }
LinkInfo CudaBackend::link(AcceleratorId) { return {}; }

ValidationResult CudaBackend::runValidation(AcceleratorId id, ValidationProfile profile) {
  ValidationResult r;
  r.profile = profile;
  const int n = 1 << 20;  // 1M floats, bounded
  if (forceValidationFailure_) {
    r.passed = false;
    r.error = failureReason_;
    r.depth = ValidationDepth::NONE;
    r.steps.push_back("synthetic failure forced");
    return r;
  }

  const int ordinal = static_cast<int>(id.get()) - 1;
  r.steps.push_back("step 1: device enumeration");
  cudaDeviceProp prop;
  if (cudaGetDeviceProperties(&prop, ordinal) != cudaSuccess) { r.passed = false; r.error = "enumerate failed"; return r; }
  r.steps.push_back("step 2: identity confirmation (compute capability " + std::to_string(prop.major) + "." + std::to_string(prop.minor) + ")");

  std::size_t freeBefore = 0, totalBefore = 0;
  cudaMemGetInfo(&freeBefore, &totalBefore);
  r.freeBefore = freeBefore;

  float* d = nullptr;
  float* h = static_cast<float*>(std::malloc(static_cast<std::size_t>(n) * sizeof(float)));
  float* out = static_cast<float*>(std::malloc(static_cast<std::size_t>(n) * sizeof(float)));
  if (!h || !out) { r.passed = false; r.error = "host alloc failed"; return r; }
  for (int i = 0; i < n; ++i) h[i] = static_cast<float>(i);

  bool ok = checkCuda(cudaSetDevice(ordinal), r, "step 3: CUDA initialization");
  if (ok) ok = checkCuda(cudaMalloc(&d, static_cast<std::size_t>(n) * sizeof(float)), r, "step 4: bounded allocation");
  if (ok) ok = checkCuda(cudaMemcpy(d, h, static_cast<std::size_t>(n) * sizeof(float), cudaMemcpyHostToDevice), r, "step 5: H2D transfer");
  if (ok) {
    r.steps.push_back("step 6: real CUDA kernel execution");
    int launchErr = ah_cuda_launch_mult2(d, d, n);
    if (launchErr != 0) { ok = false; r.error = "kernel launch failed"; }
  }
  if (ok) ok = checkCuda(cudaDeviceSynchronize(), r, "step 7: synchronization");
  if (ok) ok = checkCuda(cudaMemcpy(out, d, static_cast<std::size_t>(n) * sizeof(float), cudaMemcpyDeviceToHost), r, "step 8: D2H transfer");

  float maxErr = 0.0f;
  if (ok) {
    r.steps.push_back("step 9: CPU-reference comparison");
    for (int i = 0; i < n; ++i) {
      float reference = h[i] * 2.0f;
      float err = out[i] - reference;
      if (err < 0) err = -err;
      if (err > maxErr) maxErr = err;
    }
    if (forceNumericalMismatch_) maxErr = forcedMaxError_;
    r.maxError = maxErr;
    if (maxErr > 1e-6f) {
      ok = false;
      r.error = "numerical mismatch, max error " + std::to_string(maxErr);
    }
  }

  if (d) cudaFree(d);
  r.steps.push_back("step 10: resource cleanup");
  std::free(h);
  std::free(out);

  std::size_t freeAfter = 0, totalAfter = 0;
  cudaMemGetInfo(&freeAfter, &totalAfter);
  r.freeAfter = freeAfter;
  r.steps.push_back("step 11: memory recovery check");
  const std::uint64_t usedBefore = totalBefore > freeBefore ? totalBefore - freeBefore : 0;
  const std::uint64_t usedAfter = totalAfter > freeAfter ? totalAfter - freeAfter : 0;
  const std::uint64_t leak = usedAfter > usedBefore ? usedAfter - usedBefore : 0;
  if (leak > memoryTolerance_) {
    ok = false;
    r.error = "memory recovery check failed (leak " + std::to_string(leak) + " bytes)";
  }

  if (ok) {
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
