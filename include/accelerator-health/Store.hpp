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

#include "accelerator-health/Assessment.hpp"
#include "accelerator-health/ChangeLog.hpp"
#include "accelerator-health/Evidence.hpp"
#include "accelerator-health/History.hpp"
#include "accelerator-health/Policy.hpp"
#include "accelerator-health/StateMachine.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>

namespace ah {

struct QuarantineRecord {
  QuarantineId id{};
  Timestamp timestamp{};
  QuarantineAuthority authority = QuarantineAuthority::UNKNOWN;
  std::string reason;
  std::string triggeredBy;
  PolicyGeneration policyGeneration{};
  DeviceGeneration deviceGeneration{};
  HealthGeneration healthGeneration{};
  bool active = true;

  friend bool operator==(const QuarantineRecord&, const QuarantineRecord&) = default;
  void write(wire::Writer& w) const {
    id.write(w); timestamp.write(w); ah::write(w, authority);
    w.string(reason); w.string(triggeredBy);
    policyGeneration.write(w); deviceGeneration.write(w); healthGeneration.write(w);
    w.u8(active ? 1 : 0);
  }
  static bool read(wire::Reader& r, QuarantineRecord& q) noexcept {
    q.id = QuarantineId::read(r); q.timestamp = Timestamp::read(r);
    if (!ah::read(r, q.authority)) return false; q.reason = r.string(); q.triggeredBy = r.string();
    q.policyGeneration = PolicyGeneration::read(r);
    q.deviceGeneration = DeviceGeneration::read(r);
    q.healthGeneration = HealthGeneration::read(r);
    q.active = r.u8() != 0;
    return r.ok();
  }
};

struct RecoveryRecord {
  RecoveryId id{};
  RecoveryGeneration generation{};
  Timestamp begin{};
  std::string profile;
  std::uint32_t successfulValidations = 0;
  bool completed = false;
  Timestamp end{};

  friend bool operator==(const RecoveryRecord&, const RecoveryRecord&) = default;
  void write(wire::Writer& w) const {
    id.write(w); generation.write(w); begin.write(w);
    w.string(profile); w.u32(successfulValidations); w.u8(completed ? 1 : 0); end.write(w);
  }
  static bool read(wire::Reader& r, RecoveryRecord& rec) noexcept {
    rec.id = RecoveryId::read(r); rec.generation = RecoveryGeneration::read(r);
    rec.begin = Timestamp::read(r); rec.profile = r.string();
    rec.successfulValidations = r.u32(); rec.completed = r.u8() != 0; rec.end = Timestamp::read(r);
    return r.ok();
  }
};

struct ValidationRecord {
  ValidationRunId id{};
  AcceleratorId accelerator{};
  Timestamp timestamp{};
  ValidationProfile profile = ValidationProfile::FULL;
  ValidationDepth depth = ValidationDepth::NONE;
  bool passed = false;
  float maxError = 0.0f;
  std::string detail;
  WorkerId worker{};
  WorkerBootId workerBootId{};
  ValidationGeneration validationGeneration{};
  DeviceGeneration deviceGeneration{};

  friend bool operator==(const ValidationRecord&, const ValidationRecord&) = default;
  void write(wire::Writer& w) const {
    id.write(w); accelerator.write(w); timestamp.write(w); ah::write(w, profile); ah::write(w, depth);
    w.u8(passed ? 1 : 0); w.f32(maxError); w.string(detail);
    worker.write(w); workerBootId.write(w); validationGeneration.write(w); deviceGeneration.write(w);
  }
  static bool read(wire::Reader& r, ValidationRecord& v) noexcept {
    v.id = ValidationRunId::read(r); v.accelerator = AcceleratorId::read(r);
    v.timestamp = Timestamp::read(r); if (!ah::read(r, v.profile)) return false; if (!ah::read(r, v.depth)) return false;
    v.passed = r.u8() != 0; v.maxError = r.f32(); v.detail = r.string();
    v.worker = WorkerId::read(r); v.workerBootId = WorkerBootId::read(r);
    v.validationGeneration = ValidationGeneration::read(r);
    v.deviceGeneration = DeviceGeneration::read(r);
    return r.ok();
  }
};

struct DeviceSnapshot {
  AcceleratorId accelerator{};
  DeviceUuid uuid{};
  NodeId node{};
  WorkerId lastWorker{};
  WorkerBootId lastWorkerBootId{};
  DeviceGeneration generation{};
  HealthState state = HealthState::UNKNOWN;
  ReadinessState readiness = ReadinessState::UNKNOWN;
  ValidationDepth highestValidation = ValidationDepth::NONE;
  std::map<DimensionKind, HealthDimension> dimensions;
  Timestamp lastObservation{};
  std::vector<Fault> openFaults;
  std::uint32_t faultCount = 0;
  std::vector<ValidationRecord> validations;
  std::vector<HealthChange> changes;
  HealthAssessment latestAssessment{};
  std::optional<QuarantineRecord> quarantine;
  std::optional<RecoveryRecord> recovery;
  bool deviceGenerationChanged = false;
};

struct Accounting {
  std::uint64_t tracked = 0;
  std::uint64_t healthy = 0;
  std::uint64_t degraded = 0;
  std::uint64_t unhealthy = 0;
  std::uint64_t failed = 0;
  std::uint64_t quarantined = 0;
  std::uint64_t recovering = 0;
  std::uint64_t stale = 0;
  std::uint64_t validationRecords = 0;
  std::uint64_t unresolvedFaults = 0;
  std::uint64_t resolvedFaults = 0;
};

class HealthStore {
 public:
  explicit HealthStore(std::shared_ptr<Clock> clock,
                       HealthPolicy policy = {},
                       std::uint64_t coordinatorEpoch = 0);

  void setPolicy(const HealthPolicy& policy);
  const HealthPolicy& policy() const;
  PolicyGeneration policyGeneration() const;
  void rollCoordinatorEpoch();
  std::uint64_t coordinatorEpoch() const;
  void setCoordinatorEpoch(std::uint64_t e);

  AcceleratorId registerDevice(const DeviceUuid& uuid, const NodeId& node);
  bool isTracked(AcceleratorId id) const;
  void markLost(AcceleratorId id, const WorkerBootId& boot, std::string reason);

  std::string acceptObservation(const AcceleratorObservation& obs, const AuthorityEnvelope& env);
  void recordFault(const Fault& fault);

  void recordValidation(const ValidationRecord& v);
  void beginQuarantine(AcceleratorId id, QuarantineAuthority authority, std::string reason, std::string triggeredBy);
  void clearQuarantine(AcceleratorId id);
  void beginRecovery(AcceleratorId id);
  DeviceSnapshot revalidate(AcceleratorId id, const ValidationRecord& v);

  void reassess(AcceleratorId id);
  void reassessAll();

  DeviceSnapshot snapshot(AcceleratorId id) const;
  std::vector<AcceleratorId> deviceIds() const;
  std::vector<DeviceSnapshot> allSnapshots() const;
  std::vector<AcceleratorId> byHealthState(HealthState s) const;
  std::vector<AcceleratorId> byReadiness(ReadinessState r) const;
  std::vector<AcceleratorId> byFaultType(FaultType t) const;
  std::vector<AcceleratorId> quarantined() const;
  std::vector<AcceleratorId> revalidationRequired() const;
  std::vector<AcceleratorId> staleEvidence() const;
  std::vector<Fault> faultsOf(AcceleratorId id) const;
  std::vector<HealthChange> changesOf(AcceleratorId id) const;
  HealthAssessment assessmentOf(AcceleratorId id) const;
  std::vector<Fault> allFaults() const;

  Accounting accounting() const;
  Accounting recomputeAccounting() const;

  std::vector<std::uint8_t> encodeState() const;
  static std::vector<std::uint8_t> encode(const HealthStore& store);
  static bool decode(const std::vector<std::uint8_t>& data, HealthStore& out, std::string* error);
  bool saveToFile(const std::string& path, std::string* error) const;
  static bool loadFromFile(const std::string& path, HealthStore& out, std::string* error);

  void invalidateWorker(WorkerId worker, WorkerBootId boot, Timestamp now);
  void registerWorker(WorkerId worker, WorkerBootId boot);
  bool isWorkerCurrent(WorkerId worker, WorkerBootId boot) const;

  std::shared_ptr<Clock> clock() const { return clock_; }

  // Persistence is implemented as free functions with private access.
  friend std::vector<std::uint8_t> encodeStateInternal(const HealthStore& store);
  friend bool decodeStateInternal(const std::vector<std::uint8_t>& data, HealthStore& out, std::string* error);
  friend bool saveInternal(const std::string& path, const HealthStore& store, std::string* error);
  friend bool loadInternal(const std::string& path, HealthStore& out, std::string* error);

 private:
  struct Device {
    AcceleratorId id{};
    DeviceUuid uuid{};
    NodeId node{};
    WorkerId lastWorker{};
    WorkerBootId lastWorkerBootId{};
    DeviceGeneration generation{};
    HealthState state = HealthState::UNKNOWN;
    ReadinessState readiness = ReadinessState::UNKNOWN;
    HealthGeneration healthGeneration{};
    std::uint64_t transitionCount = 0;
    std::map<DimensionKind, HealthDimension> dimensions;
    FaultHistory faults;
    std::vector<Evidence> recentEvidence;
    std::vector<ValidationRecord> validations;
    ChangeLog changes;
    HealthAssessment latestAssessment{};
    ValidationDepth highestValidation = ValidationDepth::NONE;
    std::optional<QuarantineRecord> quarantine;
    std::optional<RecoveryRecord> recovery;
    Timestamp lastObservation{};
    Timestamp firstSeen{};
    bool deviceGenerationChangedFlag = false;
    bool policyGenerationChangedFlag = false;
    bool requiresRevalidation = false;
    bool inRecovery = false;
    bool present = true;
  };

  Device* findLocked(AcceleratorId id);
  const Device* findLocked(AcceleratorId id) const;
  DeviceSnapshot makeSnapshotLocked(const Device& d) const;
  void reassessLocked(Device& d);
  void applyTransitionLocked(Device& d, HealthState to, std::string label, std::string detail);
  void applyTransition(Device& d, HealthState to, std::string label, std::string detail);
  void recordChangeLocked(Device& d, HealthState from, HealthState to, std::string label, std::string detail);
  void updateIndexesLocked(Device& d);
  void updateIndexes(Device& d);
  void recomputeIndexes();

  std::shared_ptr<Clock> clock_;
  HealthPolicy policy_;
  PolicyGeneration policyGeneration_{1};
  std::uint64_t coordinatorEpoch_ = 0;
  std::uint64_t nextDeviceRaw_ = 1;
  std::uint64_t nextFaultRaw_ = 1;
  std::uint64_t nextObservationRaw_ = 1;
  std::uint64_t nextValidationRaw_ = 1;
  std::uint64_t nextQuarantineRaw_ = 1;
  std::uint64_t nextRecoveryRaw_ = 1;
  std::uint64_t nextAssessmentRaw_ = 1;
  std::uint64_t nextIncidentRaw_ = 1;

  mutable std::shared_mutex mu_;
  std::map<WorkerId, WorkerBootId> workerBootRegistry_;
  // Observation-generation fencing is keyed per (worker, boot) so a restarted
  // worker with a fresh boot id resumes at generation 1 while old reports from
  // a prior boot id are still rejected.
  std::map<std::pair<std::uint64_t, std::uint64_t>, ObservationGeneration> lastObservedGeneration_;
  std::map<AcceleratorId, Device> devices_;
  std::map<DeviceUuid, AcceleratorId> uuidIndex_;
  std::map<HealthState, std::set<AcceleratorId>> stateIndex_;
  std::map<ReadinessState, std::set<AcceleratorId>> readinessIndex_;
  std::map<FaultType, std::set<AcceleratorId>> faultIndex_;
  std::set<AcceleratorId> quarantineIndex_;
  std::set<AcceleratorId> staleIndex_;
  std::vector<Fault> globalFaults_;
  std::vector<HealthChange> globalChanges_;
};

}  // namespace ah
