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

#include "accelerator-health/Wire.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace ah {

// ---- Typed, printable, bounded enums with stable integer codes. ----

template <class E, std::size_t N>
constexpr const char* enum_to_string(const std::array<std::pair<E, const char*>, N>& arr, E v) noexcept {
  for (auto& kv : arr) if (kv.first == v) return kv.second;
  return "UNKNOWN";
}

template <class E, std::size_t N>
constexpr int enum_to_index(const std::array<std::pair<E, const char*>, N>& arr, E v) noexcept {
  for (std::size_t i = 0; i < N; ++i) if (arr[i].first == v) return static_cast<int>(i);
  return -1;
}

template <class E, std::size_t N>
constexpr std::optional<E> enum_from_name(const std::array<std::pair<E, const char*>, N>& arr, std::string_view s) noexcept {
  for (auto& kv : arr) if (s == kv.second) return kv.first;
  return std::nullopt;
}

template <class E, std::size_t N>
constexpr std::optional<E> enum_from_byte(const std::array<std::pair<E, const char*>, N>& arr, std::uint8_t b) noexcept {
  for (auto& kv : arr) if (static_cast<std::uint8_t>(static_cast<int>(kv.first)) == b) return kv.first;
  return std::nullopt;
}

// ---- HealthState ----
enum class HealthState : std::uint8_t {
  UNKNOWN = 0,
  INITIALIZING = 1,
  HEALTHY = 2,
  DEGRADED = 3,
  UNHEALTHY = 4,
  FAILED = 5,
  DRAINING = 6,
  DRAINED = 7,
  QUARANTINED = 8,
  RECOVERING = 9,
  REVALIDATING = 10,
  OFFLINE = 11,
  LOST = 12,
};
inline constexpr std::array<std::pair<HealthState, const char*>, 13> kHealthStateNames = {{
  {HealthState::UNKNOWN, "UNKNOWN"}, {HealthState::INITIALIZING, "INITIALIZING"},
  {HealthState::HEALTHY, "HEALTHY"}, {HealthState::DEGRADED, "DEGRADED"},
  {HealthState::UNHEALTHY, "UNHEALTHY"}, {HealthState::FAILED, "FAILED"},
  {HealthState::DRAINING, "DRAINING"}, {HealthState::DRAINED, "DRAINED"},
  {HealthState::QUARANTINED, "QUARANTINED"}, {HealthState::RECOVERING, "RECOVERING"},
  {HealthState::REVALIDATING, "REVALIDATING"}, {HealthState::OFFLINE, "OFFLINE"},
  {HealthState::LOST, "LOST"},
}};
constexpr const char* to_string(HealthState e) noexcept { return enum_to_string(kHealthStateNames, e); }
constexpr std::optional<HealthState> parse_health_state(std::string_view s) noexcept { return enum_from_name(kHealthStateNames, s); }
constexpr std::optional<HealthState> health_state_from_byte(std::uint8_t b) noexcept { return enum_from_byte(kHealthStateNames, b); }
inline void write(wire::Writer& w, HealthState e) { w.u8(static_cast<std::uint8_t>(static_cast<int>(e))); }
inline bool read(wire::Reader& r, HealthState& e) noexcept {
  auto v = health_state_from_byte(r.u8());
  if (!r.ok() || !v) { r.set_fail(); return false; }
  e = *v; return true;
}

// ---- ReadinessState ----
enum class ReadinessState : std::uint8_t {
  READY = 0,
  READY_DEGRADED = 1,
  NOT_READY = 2,
  REVALIDATION_REQUIRED = 3,
  DRAIN_REQUIRED = 4,
  QUARANTINED = 5,
  UNKNOWN = 6,
};
inline constexpr std::array<std::pair<ReadinessState, const char*>, 7> kReadinessNames = {{
  {ReadinessState::READY, "READY"}, {ReadinessState::READY_DEGRADED, "READY_DEGRADED"},
  {ReadinessState::NOT_READY, "NOT_READY"}, {ReadinessState::REVALIDATION_REQUIRED, "REVALIDATION_REQUIRED"},
  {ReadinessState::DRAIN_REQUIRED, "DRAIN_REQUIRED"}, {ReadinessState::QUARANTINED, "QUARANTINED"},
  {ReadinessState::UNKNOWN, "UNKNOWN"},
}};
constexpr const char* to_string(ReadinessState e) noexcept { return enum_to_string(kReadinessNames, e); }
constexpr std::optional<ReadinessState> parse_readiness(std::string_view s) noexcept { return enum_from_name(kReadinessNames, s); }
constexpr std::optional<ReadinessState> readiness_from_byte(std::uint8_t b) noexcept { return enum_from_byte(kReadinessNames, b); }
inline void write(wire::Writer& w, ReadinessState e) { w.u8(static_cast<std::uint8_t>(static_cast<int>(e))); }
inline bool read(wire::Reader& r, ReadinessState& e) noexcept {
  auto v = readiness_from_byte(r.u8());
  if (!r.ok() || !v) { r.set_fail(); return false; }
  e = *v; return true;
}

// ---- Severity: separate from health state. ----
enum class Severity : std::uint8_t {
  INFO = 0,
  WARNING = 1,
  DEGRADED = 2,
  CRITICAL = 3,
  FATAL = 4,
};
inline constexpr std::array<std::pair<Severity, const char*>, 5> kSeverityNames = {{
  {Severity::INFO, "INFO"}, {Severity::WARNING, "WARNING"}, {Severity::DEGRADED, "DEGRADED"},
  {Severity::CRITICAL, "CRITICAL"}, {Severity::FATAL, "FATAL"},
}};
constexpr const char* to_string(Severity e) noexcept { return enum_to_string(kSeverityNames, e); }
constexpr std::optional<Severity> parse_severity(std::string_view s) noexcept { return enum_from_name(kSeverityNames, s); }
constexpr std::optional<Severity> severity_from_byte(std::uint8_t b) noexcept { return enum_from_byte(kSeverityNames, b); }
constexpr int severity_rank(Severity s) noexcept { return static_cast<int>(s); }
inline void write(wire::Writer& w, Severity e) { w.u8(static_cast<std::uint8_t>(static_cast<int>(e))); }
inline bool read(wire::Reader& r, Severity& e) noexcept {
  auto v = severity_from_byte(r.u8());
  if (!r.ok() || !v) { r.set_fail(); return false; }
  e = *v; return true;
}

// ---- EvidenceClass ----
enum class EvidenceClass : std::uint8_t {
  MEASURED = 0,
  REPORTED = 1,
  DERIVED = 2,
  RECONSTRUCTED = 3,
  INFERRED = 4,
  UNKNOWN = 5,
};
inline constexpr std::array<std::pair<EvidenceClass, const char*>, 6> kEvidenceNames = {{
  {EvidenceClass::MEASURED, "MEASURED"}, {EvidenceClass::REPORTED, "REPORTED"},
  {EvidenceClass::DERIVED, "DERIVED"}, {EvidenceClass::RECONSTRUCTED, "RECONSTRUCTED"},
  {EvidenceClass::INFERRED, "INFERRED"}, {EvidenceClass::UNKNOWN, "UNKNOWN"},
}};
constexpr const char* to_string(EvidenceClass e) noexcept { return enum_to_string(kEvidenceNames, e); }
constexpr std::optional<EvidenceClass> parse_evidence_class(std::string_view s) noexcept { return enum_from_name(kEvidenceNames, s); }
constexpr std::optional<EvidenceClass> evidence_class_from_byte(std::uint8_t b) noexcept { return enum_from_byte(kEvidenceNames, b); }
inline void write(wire::Writer& w, EvidenceClass e) { w.u8(static_cast<std::uint8_t>(static_cast<int>(e))); }
inline bool read(wire::Reader& r, EvidenceClass& e) noexcept {
  auto v = evidence_class_from_byte(r.u8());
  if (!r.ok() || !v) { r.set_fail(); return false; }
  e = *v; return true;
}

// ---- ValidationDepth ----
enum class ValidationDepth : std::uint8_t {
  NONE = 0,
  ENUMERATION_ONLY = 1,
  INITIALIZATION = 2,
  ALLOCATION = 3,
  TRANSFER = 4,
  EXECUTION = 5,
  NUMERICAL = 6,
  FULL = 7,
};
inline constexpr std::array<std::pair<ValidationDepth, const char*>, 8> kValidationDepthNames = {{
  {ValidationDepth::NONE, "NONE"}, {ValidationDepth::ENUMERATION_ONLY, "ENUMERATION_ONLY"},
  {ValidationDepth::INITIALIZATION, "INITIALIZATION"}, {ValidationDepth::ALLOCATION, "ALLOCATION"},
  {ValidationDepth::TRANSFER, "TRANSFER"}, {ValidationDepth::EXECUTION, "EXECUTION"},
  {ValidationDepth::NUMERICAL, "NUMERICAL"}, {ValidationDepth::FULL, "FULL"},
}};
constexpr const char* to_string(ValidationDepth e) noexcept { return enum_to_string(kValidationDepthNames, e); }
constexpr std::optional<ValidationDepth> parse_validation_depth(std::string_view s) noexcept { return enum_from_name(kValidationDepthNames, s); }
constexpr std::optional<ValidationDepth> validation_depth_from_byte(std::uint8_t b) noexcept { return enum_from_byte(kValidationDepthNames, b); }
constexpr int validation_depth_rank(ValidationDepth d) noexcept { return static_cast<int>(d); }
inline void write(wire::Writer& w, ValidationDepth e) { w.u8(static_cast<std::uint8_t>(static_cast<int>(e))); }
inline bool read(wire::Reader& r, ValidationDepth& e) noexcept {
  auto v = validation_depth_from_byte(r.u8());
  if (!r.ok() || !v) { r.set_fail(); return false; }
  e = *v; return true;
}

// ---- ValidationProfile ----
enum class ValidationProfile : std::uint8_t {
  BASIC = 0,
  MEMORY = 1,
  TRANSFER = 2,
  EXECUTION = 3,
  NUMERICAL = 4,
  FULL = 5,
};
inline constexpr std::array<std::pair<ValidationProfile, const char*>, 6> kValidationProfileNames = {{
  {ValidationProfile::BASIC, "BASIC"}, {ValidationProfile::MEMORY, "MEMORY"},
  {ValidationProfile::TRANSFER, "TRANSFER"}, {ValidationProfile::EXECUTION, "EXECUTION"},
  {ValidationProfile::NUMERICAL, "NUMERICAL"}, {ValidationProfile::FULL, "FULL"},
}};
constexpr const char* to_string(ValidationProfile e) noexcept { return enum_to_string(kValidationProfileNames, e); }
constexpr std::optional<ValidationProfile> parse_validation_profile(std::string_view s) noexcept { return enum_from_name(kValidationProfileNames, s); }
constexpr std::optional<ValidationProfile> validation_profile_from_byte(std::uint8_t b) noexcept { return enum_from_byte(kValidationProfileNames, b); }
inline void write(wire::Writer& w, ValidationProfile e) { w.u8(static_cast<std::uint8_t>(static_cast<int>(e))); }
inline bool read(wire::Reader& r, ValidationProfile& e) noexcept {
  auto v = validation_profile_from_byte(r.u8());
  if (!r.ok() || !v) { r.set_fail(); return false; }
  e = *v; return true;
}

// ---- Freshness ----
enum class FreshnessStatus : std::uint8_t {
  CURRENT = 0,
  AGING = 1,
  STALE = 2,
  EXPIRED = 3,
};
inline constexpr std::array<std::pair<FreshnessStatus, const char*>, 4> kFreshnessNames = {{
  {FreshnessStatus::CURRENT, "CURRENT"}, {FreshnessStatus::AGING, "AGING"},
  {FreshnessStatus::STALE, "STALE"}, {FreshnessStatus::EXPIRED, "EXPIRED"},
}};
constexpr const char* to_string(FreshnessStatus e) noexcept { return enum_to_string(kFreshnessNames, e); }
constexpr std::optional<FreshnessStatus> parse_freshness(std::string_view s) noexcept { return enum_from_name(kFreshnessNames, s); }
constexpr std::optional<FreshnessStatus> freshness_from_byte(std::uint8_t b) noexcept { return enum_from_byte(kFreshnessNames, b); }
inline void write(wire::Writer& w, FreshnessStatus e) { w.u8(static_cast<std::uint8_t>(static_cast<int>(e))); }
inline bool read(wire::Reader& r, FreshnessStatus& e) noexcept {
  auto v = freshness_from_byte(r.u8());
  if (!r.ok() || !v) { r.set_fail(); return false; }
  e = *v; return true;
}

// ---- FaultType ----
enum class FaultType : std::uint8_t {
  ENUMERATION_FAILURE = 0,
  DRIVER_FAILURE = 1,
  CUDA_INIT_FAILURE = 2,
  ALLOCATION_FAILURE = 3,
  TRANSFER_FAILURE = 4,
  KERNEL_LAUNCH_FAILURE = 5,
  SYNCHRONIZATION_FAILURE = 6,
  NUMERICAL_MISMATCH = 7,
  MEMORY_INTEGRITY_FAILURE = 8,
  OUT_OF_MEMORY = 9,
  DEVICE_LOST = 10,
  DEVICE_RESET = 11,
  FATAL_RUNTIME_ERROR = 12,
  THERMAL_DEGRADATION = 13,
  POWER_DEGRADATION = 14,
  INTERCONNECT_DEGRADATION = 15,
  STALE_EVIDENCE = 16,
  IDENTITY_CHANGE = 17,
  UNKNOWN_FAULT = 18,
};
inline constexpr std::array<std::pair<FaultType, const char*>, 19> kFaultTypeNames = {{
  {FaultType::ENUMERATION_FAILURE, "ENUMERATION_FAILURE"}, {FaultType::DRIVER_FAILURE, "DRIVER_FAILURE"},
  {FaultType::CUDA_INIT_FAILURE, "CUDA_INIT_FAILURE"}, {FaultType::ALLOCATION_FAILURE, "ALLOCATION_FAILURE"},
  {FaultType::TRANSFER_FAILURE, "TRANSFER_FAILURE"}, {FaultType::KERNEL_LAUNCH_FAILURE, "KERNEL_LAUNCH_FAILURE"},
  {FaultType::SYNCHRONIZATION_FAILURE, "SYNCHRONIZATION_FAILURE"}, {FaultType::NUMERICAL_MISMATCH, "NUMERICAL_MISMATCH"},
  {FaultType::MEMORY_INTEGRITY_FAILURE, "MEMORY_INTEGRITY_FAILURE"}, {FaultType::OUT_OF_MEMORY, "OUT_OF_MEMORY"},
  {FaultType::DEVICE_LOST, "DEVICE_LOST"}, {FaultType::DEVICE_RESET, "DEVICE_RESET"},
  {FaultType::FATAL_RUNTIME_ERROR, "FATAL_RUNTIME_ERROR"}, {FaultType::THERMAL_DEGRADATION, "THERMAL_DEGRADATION"},
  {FaultType::POWER_DEGRADATION, "POWER_DEGRADATION"}, {FaultType::INTERCONNECT_DEGRADATION, "INTERCONNECT_DEGRADATION"},
  {FaultType::STALE_EVIDENCE, "STALE_EVIDENCE"}, {FaultType::IDENTITY_CHANGE, "IDENTITY_CHANGE"},
  {FaultType::UNKNOWN_FAULT, "UNKNOWN_FAULT"},
}};
constexpr const char* to_string(FaultType e) noexcept { return enum_to_string(kFaultTypeNames, e); }
constexpr std::optional<FaultType> parse_fault_type(std::string_view s) noexcept { return enum_from_name(kFaultTypeNames, s); }
constexpr std::optional<FaultType> fault_type_from_byte(std::uint8_t b) noexcept { return enum_from_byte(kFaultTypeNames, b); }
inline void write(wire::Writer& w, FaultType e) { w.u8(static_cast<std::uint8_t>(static_cast<int>(e))); }
inline bool read(wire::Reader& r, FaultType& e) noexcept {
  auto v = fault_type_from_byte(r.u8());
  if (!r.ok() || !v) { r.set_fail(); return false; }
  e = *v; return true;
}

// ---- FaultClass: transient vs recoverable vs fatal. ----
enum class FaultClass : std::uint8_t {
  TRANSIENT = 0,
  RECOVERABLE = 1,
  FATAL = 2,
  UNKNOWN = 3,
};
inline constexpr std::array<std::pair<FaultClass, const char*>, 4> kFaultClassNames = {{
  {FaultClass::TRANSIENT, "TRANSIENT"}, {FaultClass::RECOVERABLE, "RECOVERABLE"},
  {FaultClass::FATAL, "FATAL"}, {FaultClass::UNKNOWN, "UNKNOWN"},
}};
constexpr const char* to_string(FaultClass e) noexcept { return enum_to_string(kFaultClassNames, e); }
constexpr std::optional<FaultClass> parse_fault_class(std::string_view s) noexcept { return enum_from_name(kFaultClassNames, s); }
constexpr std::optional<FaultClass> fault_class_from_byte(std::uint8_t b) noexcept { return enum_from_byte(kFaultClassNames, b); }
inline void write(wire::Writer& w, FaultClass e) { w.u8(static_cast<std::uint8_t>(static_cast<int>(e))); }
inline bool read(wire::Reader& r, FaultClass& e) noexcept {
  auto v = fault_class_from_byte(r.u8());
  if (!r.ok() || !v) { r.set_fail(); return false; }
  e = *v; return true;
}

// ---- Health dimension kinds ----
enum class DimensionKind : std::uint8_t {
  ENUMERATION = 0,
  DRIVER = 1,
  RUNTIME_INIT = 2,
  ALLOCATION = 3,
  TRANSFER = 4,
  EXECUTION = 5,
  SYNCHRONIZATION = 6,
  NUMERICAL = 7,
  MEMORY_INTEGRITY = 8,
  MEMORY_PRESSURE = 9,
  TEMPERATURE = 10,
  POWER = 11,
  INTERCONNECT = 12,
  FATAL_ERROR = 13,
  RESTART_RESET = 14,
  FRESHNESS = 15,
  VALIDATION_CONFIDENCE = 16,
  UNKNOWN = 17,
};
inline constexpr std::array<std::pair<DimensionKind, const char*>, 18> kDimensionKindNames = {{
  {DimensionKind::ENUMERATION, "ENUMERATION"}, {DimensionKind::DRIVER, "DRIVER"},
  {DimensionKind::RUNTIME_INIT, "RUNTIME_INIT"}, {DimensionKind::ALLOCATION, "ALLOCATION"},
  {DimensionKind::TRANSFER, "TRANSFER"}, {DimensionKind::EXECUTION, "EXECUTION"},
  {DimensionKind::SYNCHRONIZATION, "SYNCHRONIZATION"}, {DimensionKind::NUMERICAL, "NUMERICAL"},
  {DimensionKind::MEMORY_INTEGRITY, "MEMORY_INTEGRITY"}, {DimensionKind::MEMORY_PRESSURE, "MEMORY_PRESSURE"},
  {DimensionKind::TEMPERATURE, "TEMPERATURE"}, {DimensionKind::POWER, "POWER"},
  {DimensionKind::INTERCONNECT, "INTERCONNECT"}, {DimensionKind::FATAL_ERROR, "FATAL_ERROR"},
  {DimensionKind::RESTART_RESET, "RESTART_RESET"}, {DimensionKind::FRESHNESS, "FRESHNESS"},
  {DimensionKind::VALIDATION_CONFIDENCE, "VALIDATION_CONFIDENCE"},
  {DimensionKind::UNKNOWN, "UNKNOWN"},
}};
constexpr const char* to_string(DimensionKind e) noexcept { return enum_to_string(kDimensionKindNames, e); }
constexpr std::optional<DimensionKind> parse_dimension_kind(std::string_view s) noexcept { return enum_from_name(kDimensionKindNames, s); }
constexpr std::optional<DimensionKind> dimension_kind_from_byte(std::uint8_t b) noexcept { return enum_from_byte(kDimensionKindNames, b); }
inline void write(wire::Writer& w, DimensionKind e) { w.u8(static_cast<std::uint8_t>(static_cast<int>(e))); }
inline bool read(wire::Reader& r, DimensionKind& e) noexcept {
  auto v = dimension_kind_from_byte(r.u8());
  if (!r.ok() || !v) { r.set_fail(); return false; }
  e = *v; return true;
}

// ---- Quarantine authority ----
enum class QuarantineAuthority : std::uint8_t {
  OPERATOR = 0,
  POLICY = 1,
  FAULT = 2,
  RECOVERY_FAILURE = 3,
  UNKNOWN = 4,
};
inline constexpr std::array<std::pair<QuarantineAuthority, const char*>, 5> kQuarantineAuthorityNames = {{
  {QuarantineAuthority::OPERATOR, "OPERATOR"}, {QuarantineAuthority::POLICY, "POLICY"},
  {QuarantineAuthority::FAULT, "FAULT"}, {QuarantineAuthority::RECOVERY_FAILURE, "RECOVERY_FAILURE"},
  {QuarantineAuthority::UNKNOWN, "UNKNOWN"},
}};
constexpr const char* to_string(QuarantineAuthority e) noexcept { return enum_to_string(kQuarantineAuthorityNames, e); }
constexpr std::optional<QuarantineAuthority> parse_quarantine_authority(std::string_view s) noexcept { return enum_from_name(kQuarantineAuthorityNames, s); }
constexpr std::optional<QuarantineAuthority> quarantine_authority_from_byte(std::uint8_t b) noexcept { return enum_from_byte(kQuarantineAuthorityNames, b); }
inline void write(wire::Writer& w, QuarantineAuthority e) { w.u8(static_cast<std::uint8_t>(static_cast<int>(e))); }
inline bool read(wire::Reader& r, QuarantineAuthority& e) noexcept {
  auto v = quarantine_authority_from_byte(r.u8());
  if (!r.ok() || !v) { r.set_fail(); return false; }
  e = *v; return true;
}

// ---- Required next action (diagnosis). ----
enum class ActionRequired : std::uint8_t {
  NONE = 0,
  MONITOR = 1,
  REVALIDATE = 2,
  DRAIN = 3,
  QUARANTINE = 4,
  RECOVER = 5,
  UNKNOWN = 6,
};
inline constexpr std::array<std::pair<ActionRequired, const char*>, 7> kActionRequiredNames = {{
  {ActionRequired::NONE, "NONE"}, {ActionRequired::MONITOR, "MONITOR"},
  {ActionRequired::REVALIDATE, "REVALIDATE"}, {ActionRequired::DRAIN, "DRAIN"},
  {ActionRequired::QUARANTINE, "QUARANTINE"}, {ActionRequired::RECOVER, "RECOVER"},
  {ActionRequired::UNKNOWN, "UNKNOWN"},
}};
constexpr const char* to_string(ActionRequired e) noexcept { return enum_to_string(kActionRequiredNames, e); }
constexpr std::optional<ActionRequired> parse_action(std::string_view s) noexcept { return enum_from_name(kActionRequiredNames, s); }
constexpr std::optional<ActionRequired> action_from_byte(std::uint8_t b) noexcept { return enum_from_byte(kActionRequiredNames, b); }
inline void write(wire::Writer& w, ActionRequired e) { w.u8(static_cast<std::uint8_t>(static_cast<int>(e))); }
inline bool read(wire::Reader& r, ActionRequired& e) noexcept {
  auto v = action_from_byte(r.u8());
  if (!r.ok() || !v) { r.set_fail(); return false; }
  e = *v; return true;
}

// ---- Dimension health state (per-DimensionKind). ----
enum class DimensionState : std::uint8_t {
  NOMINAL = 0,
  DEGRADED = 1,
  FAILED = 2,
  UNAVAILABLE = 3,
  UNKNOWN = 4,
};
inline constexpr std::array<std::pair<DimensionState, const char*>, 5> kDimensionStateNames = {{
  {DimensionState::NOMINAL, "NOMINAL"}, {DimensionState::DEGRADED, "DEGRADED"},
  {DimensionState::FAILED, "FAILED"}, {DimensionState::UNAVAILABLE, "UNAVAILABLE"},
  {DimensionState::UNKNOWN, "UNKNOWN"},
}};
constexpr const char* to_string(DimensionState e) noexcept { return enum_to_string(kDimensionStateNames, e); }
constexpr std::optional<DimensionState> parse_dimension_state(std::string_view s) noexcept { return enum_from_name(kDimensionStateNames, s); }
constexpr std::optional<DimensionState> dimension_state_from_byte(std::uint8_t b) noexcept { return enum_from_byte(kDimensionStateNames, b); }
inline void write(wire::Writer& w, DimensionState e) { w.u8(static_cast<std::uint8_t>(static_cast<int>(e))); }
inline bool read(wire::Reader& r, DimensionState& e) noexcept {
  auto v = dimension_state_from_byte(r.u8());
  if (!r.ok() || !v) { r.set_fail(); return false; }
  e = *v; return true;
}

}  // namespace ah
