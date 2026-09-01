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

#include "accelerator-health/Enums.hpp"
#include "accelerator-health/StrongIdentity.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ah {

struct DeviceIdentity {
  AcceleratorId id{};
  DeviceUuid uuid{};
  DeviceGeneration generation{};
  std::string name;
  int computeCapabilityMajor = 0;
  int computeCapabilityMinor = 0;
  std::uint64_t totalMemory = 0;
  std::uint64_t freeMemory = 0;
  bool supported = true;  // false = unsupported by this runtime (distinct from healthy)

  friend bool operator==(const DeviceIdentity&, const DeviceIdentity&) = default;
};

struct MemoryInfo { bool available = false; std::uint64_t total = 0; std::uint64_t free = 0; std::uint64_t used = 0; };
struct ThermalInfo { bool available = false; double tempC = 0.0; };
struct PowerInfo { bool available = false; double watts = 0.0; };
struct ErrorInfo { bool available = false; std::uint64_t uncorrectableEcc = 0; std::uint64_t correctableEcc = 0; };
struct LinkInfo { bool available = false; std::uint32_t width = 0; std::uint64_t throughput = 0; std::string type; };
struct RuntimeInfo { bool cudaAvailable = false; std::string version; std::string driver; };

struct ValidationResult {
  bool passed = false;
  ValidationProfile profile = ValidationProfile::FULL;
  ValidationDepth depth = ValidationDepth::NONE;
  float maxError = 0.0f;
  std::uint64_t freeBefore = 0;
  std::uint64_t freeAfter = 0;
  std::string error;
  std::vector<std::string> steps;  // evidence records, in deterministic order
};

// Backend abstraction for observing accelerators and running validation.
// Unsupported observations are reported with available=false and must never be
// treated as healthy evidence.
class Backend {
 public:
  virtual ~Backend() = default;
  virtual const char* name() const = 0;

  virtual std::vector<DeviceIdentity> enumerate() = 0;
  virtual bool queryIdentity(AcceleratorId id, DeviceIdentity& out) = 0;

  virtual bool isCudaRuntimeAvailable() = 0;
  virtual RuntimeInfo runtime() = 0;

  virtual MemoryInfo memory(AcceleratorId id) = 0;
  virtual ThermalInfo temperature(AcceleratorId id) = 0;
  virtual PowerInfo power(AcceleratorId id) = 0;
  virtual ErrorInfo errors(AcceleratorId id) = 0;
  virtual LinkInfo link(AcceleratorId id) = 0;

  virtual ValidationResult runValidation(AcceleratorId id, ValidationProfile profile) = 0;
};

}  // namespace ah
