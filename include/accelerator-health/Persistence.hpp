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

#include "accelerator-health/Store.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ah {

// Versioned, deterministic, integrity-checked binary persistence. Implemented in
// Persistence.cpp. See that file for the schema; the public format is covered by
// corruption/truncation/trailing-garbage rejection tests.
inline constexpr std::uint32_t kPersistenceSchemaVersion = 1;

std::vector<std::uint8_t> encodeStateInternal(const HealthStore& store);
bool decodeStateInternal(const std::vector<std::uint8_t>& data, HealthStore& out, std::string* error);
bool saveInternal(const std::string& path, const HealthStore& store, std::string* error);
bool loadInternal(const std::string& path, HealthStore& out, std::string* error);

}  // namespace ah
