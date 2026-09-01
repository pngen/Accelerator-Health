// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include "accelerator-health/Backend.hpp"

#include <map>

namespace ah {

// Real NVIDIA/CUDA backend. Requires a CUDA-capable GPU (target
// architecture: RTX 5090 / sm_120, compute capability 12.0). Implemented in
// src/cuda; only defined when AH_HAS_CUDA is true.
//
// The full validation sequence performs the documented 11-step proof and the
// implementation verifies actual memory accounting against baseline.
class CudaBackend final : public Backend {
 public:
  const char* name() const override { return "cuda"; }

  std::vector<DeviceIdentity> enumerate() override;
  bool queryIdentity(AcceleratorId id, DeviceIdentity& out) override;
  bool isCudaRuntimeAvailable() override;
  RuntimeInfo runtime() override;
  MemoryInfo memory(AcceleratorId id) override;
  ThermalInfo temperature(AcceleratorId id) override;
  PowerInfo power(AcceleratorId id) override;
  ErrorInfo errors(AcceleratorId id) override;
  LinkInfo link(AcceleratorId id) override;
  ValidationResult runValidation(AcceleratorId id, ValidationProfile profile) override;

  // Controlled fault injection (does not touch physical hardware).
  void setForceValidationFailure(bool on, std::string reason = "synthetic validation failure");
  void setForceNumericalMismatch(bool on, float maxError = 0.5f);
  void setMemoryRecoveryTolerance(std::uint64_t bytes);

 private:
  bool forceValidationFailure_ = false;
  std::string failureReason_;
  bool forceNumericalMismatch_ = false;
  float forcedMaxError_ = 0.5f;
  std::uint64_t memoryTolerance_ = 4 * 1024 * 1024;
};

}  // namespace ah
