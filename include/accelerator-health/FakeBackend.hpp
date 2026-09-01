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

// Deterministic, in-memory backend used for tests and for scenarios that must
// not touch real GPU hardware. Supports deliberate fault injection at the
// backend abstraction so negative health paths can be exercised safely.
class FakeBackend final : public Backend {
 public:
  explicit FakeBackend(bool cudaAvailable = false);

  const char* name() const override { return "fake"; }

  void addDevice(const DeviceIdentity& d) { devices_[d.id] = d; }
  void removeDevice(AcceleratorId id) { devices_.erase(id); }

  void setMemory(const MemoryInfo& m) { mem_ = m; }
  void setTemperature(const ThermalInfo& t) { temp_ = t; }
  void setPower(const PowerInfo& p) { power_ = p; }
  void setErrors(const ErrorInfo& e) { errors_ = e; }
  void setLink(const LinkInfo& l) { link_ = l; }
  void setRuntime(const RuntimeInfo& r) { runtime_ = r; }

  void setCudaAvailability(bool avail) { cudaAvailable_ = avail; }

  // Fault injection knobs.
  void setForceValidationFailure(bool on, std::string reason = "synthetic validation failure");
  void setForceNumericalMismatch(bool on, float maxError = 0.5f);
  void setForcedMaxError(float e) { forcedMaxError_ = e; }
  void setValidationStepLimit(int steps) { stepsLimit_ = steps; }

  std::vector<DeviceIdentity> enumerate() override;
  bool queryIdentity(AcceleratorId id, DeviceIdentity& out) override;
  bool isCudaRuntimeAvailable() override { return cudaAvailable_; }
  RuntimeInfo runtime() override { return runtime_; }
  MemoryInfo memory(AcceleratorId id) override;
  ThermalInfo temperature(AcceleratorId id) override;
  PowerInfo power(AcceleratorId id) override;
  ErrorInfo errors(AcceleratorId id) override;
  LinkInfo link(AcceleratorId id) override;
  ValidationResult runValidation(AcceleratorId id, ValidationProfile profile) override;

 private:
  bool cudaAvailable_;
  std::map<AcceleratorId, DeviceIdentity> devices_;
  MemoryInfo mem_;
  ThermalInfo temp_;
  PowerInfo power_;
  ErrorInfo errors_;
  LinkInfo link_;
  RuntimeInfo runtime_;
  bool forceValidationFailure_ = false;
  std::string failureReason_;
  bool forceNumericalMismatch_ = false;
  float forcedMaxError_ = 0.5f;
  int stepsLimit_ = 0;
  std::uint64_t allocationCounter_ = 0;
};

}  // namespace ah
