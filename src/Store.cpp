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
#include "accelerator-health/Store.hpp"
#include "accelerator-health/Persistence.hpp"

#include "accelerator-health/Freshness.hpp"
#include "accelerator-health/Util.hpp"

#include <algorithm>
#include <stdexcept>

namespace ah {

namespace {
HealthGeneration bumpGeneration(HealthGeneration g) { return g.next(); }
}  // namespace

HealthStore::HealthStore(std::shared_ptr<Clock> clock, HealthPolicy policy, std::uint64_t epoch)
    : clock_(std::move(clock)), policy_(std::move(policy)), coordinatorEpoch_(epoch) {}

void HealthStore::setPolicy(const HealthPolicy& policy) {
  {
    std::unique_lock lock(mu_);
    policy_ = policy;
    ++policyGeneration_;
    for (auto& [id, d] : devices_) {
      (void)id;
      d.policyGenerationChangedFlag = true;
    }
  }
  reassessAll();
}
const HealthPolicy& HealthStore::policy() const {
  std::shared_lock lock(mu_);
  return policy_;
}
PolicyGeneration HealthStore::policyGeneration() const {
  std::shared_lock lock(mu_);
  return policyGeneration_;
}
void HealthStore::rollCoordinatorEpoch() {
  std::unique_lock lock(mu_);
  ++coordinatorEpoch_;
}
std::uint64_t HealthStore::coordinatorEpoch() const {
  std::shared_lock lock(mu_);
  return coordinatorEpoch_;
}
void HealthStore::setCoordinatorEpoch(std::uint64_t e) {
  std::unique_lock lock(mu_);
  coordinatorEpoch_ = e;
}

void HealthStore::registerWorker(WorkerId worker, WorkerBootId boot) {
  std::unique_lock lock(mu_);
  workerBootRegistry_[worker] = boot;
}
bool HealthStore::isWorkerCurrent(WorkerId worker, WorkerBootId boot) const {
  std::shared_lock lock(mu_);
  auto it = workerBootRegistry_.find(worker);
  return it != workerBootRegistry_.end() && it->second == boot;
}
void HealthStore::invalidateWorker(WorkerId worker, WorkerBootId boot, Timestamp now) {
  (void)now;
  std::unique_lock lock(mu_);
  auto it = workerBootRegistry_.find(worker);
  if (it != workerBootRegistry_.end() && it->second == boot) {
    // Mark devices last observed by this worker as requiring revalidation.
    for (auto& [id, d] : devices_) {
      if (d.lastWorker == worker && d.lastWorkerBootId == boot) {
        d.lastObservation = Timestamp{};  // strip freshness so readiness drops
        d.deviceGenerationChangedFlag = false;
        reassessLocked(d);
        (void)id;
      }
    }
    workerBootRegistry_.erase(it);
  }
}

AcceleratorId HealthStore::registerDevice(const DeviceUuid& uuid, const NodeId& node) {
  std::unique_lock lock(mu_);
  auto existing = uuidIndex_.find(uuid);
  if (existing != uuidIndex_.end()) return existing->second;
  // A non-zero uuid determines a stable device id so distributed workers agree.
  const AcceleratorId id = uuid.get() != 0 ? AcceleratorId{uuid.get()} : AcceleratorId{nextDeviceRaw_++};
  Device d;
  d.id = id;
  d.uuid = uuid;
  d.node = node;
  d.generation = DeviceGeneration{1};
  d.state = HealthState::INITIALIZING;
  d.firstSeen = clock_->now();
  d.healthGeneration = HealthGeneration{1};
  devices_[id] = std::move(d);
  uuidIndex_[uuid] = id;
  updateIndexesLocked(devices_[id]);
  return id;
}

bool HealthStore::isTracked(AcceleratorId id) const {
  std::shared_lock lock(mu_);
  return devices_.find(id) != devices_.end();
}

void HealthStore::markLost(AcceleratorId id, const WorkerBootId& boot, std::string reason) {
  (void)boot;
  std::unique_lock lock(mu_);
  auto it = devices_.find(id);
  if (it == devices_.end()) return;
  Device& d = it->second;
  d.present = false;
  if (d.state != HealthState::LOST) {
    if (canTransition(d.state, HealthState::LOST)) {
      applyTransitionLocked(d, HealthState::LOST, "lost", std::move(reason));
    } else {
      d.state = HealthState::LOST;
      recordChangeLocked(d, d.state, d.state, "lost", std::move(reason));
    }
  }
}

// ---- Fencing ----
std::string HealthStore::acceptObservation(const AcceleratorObservation& obs, const AuthorityEnvelope& env) {
  std::unique_lock lock(mu_);
  auto it = devices_.find(obs.accelerator);
  if (it == devices_.end()) return "rejected: unknown device";
  Device& d = it->second;
  if (env.coordinatorEpoch != coordinatorEpoch_) return "rejected: stale coordinator epoch";
  auto wb = workerBootRegistry_.find(env.workerId);
  if (wb == workerBootRegistry_.end()) return "rejected: unregistered worker";
  if (wb->second != env.workerBootId) return "rejected: stale worker boot id";
  const auto ogKey = std::make_pair(env.workerId.get(), env.workerBootId.get());
  auto og = lastObservedGeneration_.find(ogKey);
  if (og != lastObservedGeneration_.end() && env.observationGeneration <= og->second)
    return "rejected: stale observation generation";
  if (env.deviceGeneration.get() < d.generation.get()) return "rejected: stale device generation";

  if (env.deviceGeneration > d.generation) {
    d.generation = env.deviceGeneration;
    d.deviceGenerationChangedFlag = true;
  }
  d.lastWorker = obs.worker;
  d.lastWorkerBootId = obs.workerBootId;
  d.requiresRevalidation = false;
  for (const auto& dim : obs.dimensions) d.dimensions[dim.kind] = dim;
  d.lastObservation = obs.timestamp;
  lastObservedGeneration_[ogKey] = env.observationGeneration;
  reassessLocked(d);
  return "accepted";
}

void HealthStore::recordFault(const Fault& fault) {
  std::unique_lock lock(mu_);
  auto it = devices_.find(fault.accelerator);
  if (it == devices_.end()) return;  // unknown device; do not fabricate evidence
  Device& d = it->second;
  d.faults.record(fault);
  faultIndex_[fault.type].insert(fault.accelerator);
  globalFaults_.push_back(fault);
  reassessLocked(d);
}

void HealthStore::recordValidation(const ValidationRecord& v) {
  std::unique_lock lock(mu_);
  auto it = devices_.find(v.accelerator);
  if (it == devices_.end()) return;
  Device& d = it->second;
  d.validations.push_back(v);
  if (v.passed && static_cast<int>(v.depth) > static_cast<int>(d.highestValidation)) {
    d.highestValidation = v.depth;
  }
  reassessLocked(d);
}

// ---- Quarantine / recovery ----
void HealthStore::beginQuarantine(AcceleratorId id, QuarantineAuthority authority, std::string reason, std::string triggeredBy) {
  std::unique_lock lock(mu_);
  auto it = devices_.find(id);
  if (it == devices_.end()) throw std::invalid_argument("quarantine: unknown device");
  Device& d = it->second;
  if (d.quarantine && d.quarantine->active) throw std::logic_error("quarantine already active");  // duplicate active quarantine
  QuarantineRecord q;
  q.id = QuarantineId{nextQuarantineRaw_++};
  q.timestamp = clock_->now();
  q.authority = authority;
  q.reason = std::move(reason);
  q.triggeredBy = std::move(triggeredBy);
  q.policyGeneration = policyGeneration_;
  q.deviceGeneration = d.generation;
  q.healthGeneration = d.healthGeneration;
  q.active = true;
  d.quarantine = q;
  quarantineIndex_.insert(id);
  if (d.state != HealthState::QUARANTINED) {
    applyTransitionLocked(d, HealthState::QUARANTINED, "quarantine", q.reason);
  }
  reassessLocked(d);
}

void HealthStore::clearQuarantine(AcceleratorId id) {
  std::unique_lock lock(mu_);
  auto it = devices_.find(id);
  if (it == devices_.end()) return;
  Device& d = it->second;
  if (d.quarantine) {
    d.quarantine->active = false;
    quarantineIndex_.erase(id);
  }
  reassessLocked(d);
}

void HealthStore::beginRecovery(AcceleratorId id) {
  std::unique_lock lock(mu_);
  auto it = devices_.find(id);
  if (it == devices_.end()) throw std::invalid_argument("recovery: unknown device");
  Device& d = it->second;
  if (d.inRecovery) return;
  if (!canTransition(d.state, HealthState::RECOVERING)) {
    // Allow from QUARANTINED, FAILED, DRAINED.
    throw InvalidTransition(d.state, HealthState::RECOVERING);
  }
  applyTransitionLocked(d, HealthState::RECOVERING, "recovery", "begin recovery");
  d.inRecovery = true;
  RecoveryRecord rec;
  rec.id = RecoveryId{nextRecoveryRaw_++};
  rec.generation = d.recovery ? d.recovery->generation.next() : RecoveryGeneration{1};
  rec.begin = clock_->now();
  rec.profile = to_string(policy_.revalidationProfile);
  d.recovery = rec;
  reassessLocked(d);
}

DeviceSnapshot HealthStore::revalidate(AcceleratorId id, const ValidationRecord& v) {
  std::unique_lock lock(mu_);
  auto it = devices_.find(id);
  if (it == devices_.end()) throw std::invalid_argument("revalidate: unknown device");
  Device& d = it->second;
  d.validations.push_back(v);
  if (v.passed && static_cast<int>(v.depth) > static_cast<int>(d.highestValidation)) d.highestValidation = v.depth;
  if (d.inRecovery) {
    if (v.passed) {
      if (d.state == HealthState::RECOVERING && canTransition(d.state, HealthState::REVALIDATING)) {
        applyTransitionLocked(d, HealthState::REVALIDATING, "revalidate", "enter revalidation");
      }
      if (d.recovery) ++d.recovery->successfulValidations;
      if (d.state == HealthState::REVALIDATING && d.recovery &&
          d.recovery->successfulValidations >= policy_.recoveryValidationsRequired) {
        applyTransitionLocked(d, HealthState::HEALTHY, "recovery", "revalidated");
        d.inRecovery = false;
        if (d.recovery) { d.recovery->completed = true; d.recovery->end = clock_->now(); }
      }
    } else {
      // Failed validation during recovery is conservative: quarantine.
      if (d.state == HealthState::RECOVERING || d.state == HealthState::REVALIDATING) {
        applyTransitionLocked(d, HealthState::QUARANTINED, "revalidate-failed", "validation failed during recovery");
      }
    }
  }
  reassessLocked(d);
  return makeSnapshotLocked(d);
}

// ---- Reassessment ----
void HealthStore::reassess(AcceleratorId id) {
  std::unique_lock lock(mu_);
  auto it = devices_.find(id);
  if (it == devices_.end()) return;
  reassessLocked(it->second);
}
void HealthStore::reassessAll() {
  std::unique_lock lock(mu_);
  for (auto& [id, d] : devices_) { (void)id; reassessLocked(d); }
}

// ---- Reassessment internals (lock held) ----
void HealthStore::reassessLocked(Device& d) {
  AssessmentEngine engine;
  AssessmentInput in;
  in.accelerator = d.id;
  in.uuid = d.uuid;
  in.deviceGeneration = d.generation;
  in.currentState = d.state;
  in.quarantined = d.quarantine && d.quarantine->active;
  in.inRecovery = d.inRecovery;
  in.now = clock_->now();
  in.dimensions = d.dimensions;
  in.faultStats = d.faults.stats();
  in.highestValidationDepth = d.highestValidation;
  in.lastObservation = d.lastObservation;
  in.policy = policy_;
  in.deviceGenerationChanged = d.deviceGenerationChangedFlag;
  in.policyGenerationChanged = d.policyGenerationChangedFlag;

  HealthAssessment desired = engine.assess(in);
  const HealthState target = desired.state;

  bool transitionBlocked = false;
  if (target != d.state) {
    if (canTransition(d.state, target)) {
      applyTransitionLocked(d, target, "health", to_string(target));
    } else {
      // Cannot reach the evidence-based state directly; require recovery/revalidation.
      transitionBlocked = true;
    }
  }

  // Freshness for the actual state.
  FreshnessEvaluator feval(policy_.freshness);
  const bool haveEvidence = d.lastObservation.nanos != 0;
  const FreshnessStatus freshness = haveEvidence ? feval.evaluate(d.lastObservation, in.now) : FreshnessStatus::EXPIRED;

  ReadinessInput ri;
  ri.health = d.state;
  ri.freshness = freshness;
  ri.validationDepth = d.highestValidation;
  ri.requiredValidationDepth = policy_.requiredValidationDepth;
  ri.quarantined = d.quarantine && d.quarantine->active;
  ri.inRecovery = d.inRecovery;
  ri.deviceGenerationChanged = d.deviceGenerationChangedFlag;
  ri.policyGenerationChanged = d.policyGenerationChangedFlag;
  ri.requiresRevalidation = d.requiresRevalidation;
  ri.allowDegradedReadiness = policy_.allowDegradedReadiness;
  ReadinessResult rr = ReadinessEvaluator{}.evaluate(ri);
  if (transitionBlocked) {
    rr.state = ReadinessState::REVALIDATION_REQUIRED;
    rr.executionReady = false;
  }

  HealthAssessment final_a = desired;
  final_a.state = d.state;
  final_a.readiness = rr.state;
  final_a.executionReady = rr.executionReady;
  final_a.validationDepth = d.highestValidation;
  final_a.healthGeneration = d.healthGeneration;
  final_a.policyGeneration = policyGeneration_;
  final_a.deviceGeneration = d.generation;
  final_a.quarantined = d.quarantine && d.quarantine->active;
  final_a.inRecovery = d.inRecovery;
  d.readiness = rr.state;
  d.latestAssessment = final_a;

  // Stale index.
  if (freshness == FreshnessStatus::STALE || freshness == FreshnessStatus::EXPIRED) staleIndex_.insert(d.id);
  else staleIndex_.erase(d.id);
  updateIndexesLocked(d);
}

// ---- Transitions / indexes (lock held) ----
void HealthStore::applyTransitionLocked(Device& d, HealthState to, std::string label, std::string detail) {
  if (d.state == to) return;
  if (!canTransition(d.state, to)) throw InvalidTransition(d.state, to);
  const HealthState from = d.state;
  d.state = to;
  ++d.transitionCount;
  d.healthGeneration = bumpGeneration(d.healthGeneration);
  recordChangeLocked(d, from, to, std::move(label), std::move(detail));
}

void HealthStore::recordChangeLocked(Device& d, HealthState from, HealthState to, std::string label, std::string detail) {
  HealthChange hc;
  hc.timestamp = clock_->now();
  hc.accelerator = d.id;
  hc.from = from;
  hc.to = to;
  hc.readinessFrom = d.readiness;
  hc.readinessTo = d.readiness;
  hc.label = std::move(label);
  hc.detail = std::move(detail);
  hc.healthGeneration = d.healthGeneration;
  hc.policyGeneration = policyGeneration_;
  d.changes.append(std::move(hc));
  globalChanges_.push_back(d.changes.all().back());
}

void HealthStore::updateIndexesLocked(Device& d) {
  for (auto& kv : stateIndex_) if (kv.second.erase(d.id) > 0) break;
  stateIndex_[d.state].insert(d.id);
  for (auto& kv : readinessIndex_) if (kv.second.erase(d.id) > 0) break;
  readinessIndex_[d.readiness].insert(d.id);
}

void HealthStore::recomputeIndexes() {
  stateIndex_.clear();
  readinessIndex_.clear();
  faultIndex_.clear();
  quarantineIndex_.clear();
  staleIndex_.clear();
  for (auto& [id, d] : devices_) {
    (void)id;
    stateIndex_[d.state].insert(d.id);
    readinessIndex_[d.readiness].insert(d.id);
    auto open = d.faults.openFaults();
    for (const auto& f : open) faultIndex_[f.type].insert(d.id);
    if (d.quarantine && d.quarantine->active) quarantineIndex_.insert(d.id);
    // stale index set during reassess index scan below
  }
}

// ---- Queries ----
HealthStore::Device* HealthStore::findLocked(AcceleratorId id) {
  auto it = devices_.find(id);
  return it == devices_.end() ? nullptr : &it->second;
}
const HealthStore::Device* HealthStore::findLocked(AcceleratorId id) const {
  auto it = devices_.find(id);
  return it == devices_.end() ? nullptr : &it->second;
}

DeviceSnapshot HealthStore::makeSnapshotLocked(const HealthStore::Device& d) const {
  DeviceSnapshot s;
  s.accelerator = d.id;
  s.uuid = d.uuid;
  s.node = d.node;
  s.lastWorker = d.lastWorker;
  s.lastWorkerBootId = d.lastWorkerBootId;
  s.generation = d.generation;
  s.state = d.state;
  s.readiness = d.readiness;
  s.highestValidation = d.highestValidation;
  s.dimensions = d.dimensions;
  s.lastObservation = d.lastObservation;
  s.openFaults = d.faults.openFaults();
  s.faultCount = static_cast<std::uint32_t>(d.faults.size());
  s.validations = d.validations;
  s.changes = d.changes.all();
  s.latestAssessment = d.latestAssessment;
  s.quarantine = d.quarantine;
  s.recovery = d.recovery;
  s.deviceGenerationChanged = d.deviceGenerationChangedFlag;
  return s;
}

DeviceSnapshot HealthStore::snapshot(AcceleratorId id) const {
  std::shared_lock lock(mu_);
  const Device* d = findLocked(id);
  if (!d) return {};
  return makeSnapshotLocked(*d);
}
std::vector<AcceleratorId> HealthStore::deviceIds() const {
  std::shared_lock lock(mu_);
  std::vector<AcceleratorId> out;
  out.reserve(devices_.size());
  for (const auto& [id, d] : devices_) { (void)d; out.push_back(id); }
  return out;
}
std::vector<DeviceSnapshot> HealthStore::allSnapshots() const {
  std::shared_lock lock(mu_);
  std::vector<DeviceSnapshot> out;
  out.reserve(devices_.size());
  for (const auto& [id, d] : devices_) { (void)id; out.push_back(makeSnapshotLocked(d)); }
  return out;
}
std::vector<AcceleratorId> HealthStore::byHealthState(HealthState s) const {
  std::shared_lock lock(mu_);
  auto it = stateIndex_.find(s);
  if (it == stateIndex_.end()) return {};
  return std::vector<AcceleratorId>(it->second.begin(), it->second.end());
}
std::vector<AcceleratorId> HealthStore::byReadiness(ReadinessState r) const {
  std::shared_lock lock(mu_);
  auto it = readinessIndex_.find(r);
  if (it == readinessIndex_.end()) return {};
  return std::vector<AcceleratorId>(it->second.begin(), it->second.end());
}
std::vector<AcceleratorId> HealthStore::byFaultType(FaultType t) const {
  std::shared_lock lock(mu_);
  auto it = faultIndex_.find(t);
  if (it == faultIndex_.end()) return {};
  return std::vector<AcceleratorId>(it->second.begin(), it->second.end());
}
std::vector<AcceleratorId> HealthStore::quarantined() const {
  std::shared_lock lock(mu_);
  return std::vector<AcceleratorId>(quarantineIndex_.begin(), quarantineIndex_.end());
}
std::vector<AcceleratorId> HealthStore::revalidationRequired() const {
  std::shared_lock lock(mu_);
  std::vector<AcceleratorId> out;
  for (const auto& [id, d] : devices_) {
    if (d.readiness == ReadinessState::REVALIDATION_REQUIRED || d.readiness == ReadinessState::NOT_READY) out.push_back(id);
  }
  return out;
}
std::vector<AcceleratorId> HealthStore::staleEvidence() const {
  std::shared_lock lock(mu_);
  return std::vector<AcceleratorId>(staleIndex_.begin(), staleIndex_.end());
}
std::vector<Fault> HealthStore::faultsOf(AcceleratorId id) const {
  std::shared_lock lock(mu_);
  const Device* d = findLocked(id);
  if (!d) return {};
  return d->faults.all();
}
std::vector<HealthChange> HealthStore::changesOf(AcceleratorId id) const {
  std::shared_lock lock(mu_);
  const Device* d = findLocked(id);
  if (!d) return {};
  return d->changes.all();
}
HealthAssessment HealthStore::assessmentOf(AcceleratorId id) const {
  std::shared_lock lock(mu_);
  const Device* d = findLocked(id);
  if (!d) return {};
  return d->latestAssessment;
}
std::vector<Fault> HealthStore::allFaults() const {
  std::shared_lock lock(mu_);
  return globalFaults_;
}

// ---- Accounting ----
Accounting HealthStore::accounting() const { return recomputeAccounting(); }
Accounting HealthStore::recomputeAccounting() const {
  std::shared_lock lock(mu_);
  Accounting a;
  for (const auto& [id, d] : devices_) {
    (void)id;
    ++a.tracked;
    if (d.state == HealthState::HEALTHY) ++a.healthy;
    else if (d.state == HealthState::DEGRADED) ++a.degraded;
    else if (d.state == HealthState::UNHEALTHY) ++a.unhealthy;
    else if (d.state == HealthState::FAILED) ++a.failed;
    if (d.state == HealthState::QUARANTINED) ++a.quarantined;
    if (d.inRecovery) ++a.recovering;
    a.validationRecords += d.validations.size();
    for (const auto& f : d.faults.all()) {
      if (f.resolution == ResolutionState::OPEN) ++a.unresolvedFaults;
      else ++a.resolvedFaults;
    }
  }
  // stale
  for (const auto& [id, d] : devices_) {
    (void)id;
    FreshnessEvaluator feval(policy_.freshness);
    const bool haveEvidence = d.lastObservation.nanos != 0;
    const FreshnessStatus fr = haveEvidence ? feval.evaluate(d.lastObservation, clock_->now()) : FreshnessStatus::EXPIRED;
    if (fr == FreshnessStatus::STALE || fr == FreshnessStatus::EXPIRED) ++a.stale;
  }
  return a;
}

// ---- Persistence (delegates to Persistence.cpp) ----
std::vector<std::uint8_t> HealthStore::encodeState() const { return encodeStateInternal(*this); }
std::vector<std::uint8_t> HealthStore::encode(const HealthStore& store) { return encodeStateInternal(store); }
bool HealthStore::decode(const std::vector<std::uint8_t>& data, HealthStore& out, std::string* error) {
  return decodeStateInternal(data, out, error);
}
bool HealthStore::saveToFile(const std::string& path, std::string* error) const { return saveInternal(path, *this, error); }
bool HealthStore::loadFromFile(const std::string& path, HealthStore& out, std::string* error) {
  return loadInternal(path, out, error);
}

}  // namespace ah
