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
#include "accelerator-health/Persistence.hpp"
#include "accelerator-health/Time.hpp"
#include "accelerator-health/Util.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <filesystem>
#include <fstream>
#include <set>

namespace ah {

namespace {
constexpr std::uint8_t kMagicP[4] = {0x41, 0x48, 0x50, 0x53};  // "AHPS"

void writeAssessment(wire::Writer& w, const HealthAssessment& a) {
  a.id.write(w);
  a.accelerator.write(w);
  a.uuid.write(w);
  write(w, a.state);
  write(w, a.readiness);
  write(w, a.validationDepth);
  w.f32(a.confidence);
  write(w, a.action);
  w.u8(a.executionReady ? 1 : 0);
  w.u8(a.quarantined ? 1 : 0);
  a.quarantineId.write(w);
  w.u8(a.inRecovery ? 1 : 0);
  a.timestamp.write(w);
  a.healthGeneration.write(w);
  a.policyGeneration.write(w);
  a.deviceGeneration.write(w);
  w.u32(static_cast<std::uint32_t>(a.reasons.size()));
  for (const auto& r : a.reasons) w.string(r);
  w.string(a.digest);
}
bool readAssessment(wire::Reader& r, HealthAssessment& a) noexcept {
  a.id = HealthAssessmentId::read(r);
  a.accelerator = AcceleratorId::read(r);
  a.uuid = DeviceUuid::read(r);
  if (!read(r, a.state)) return false;
  if (!read(r, a.readiness)) return false;
  if (!read(r, a.validationDepth)) return false;
  a.confidence = r.f32();
  if (!read(r, a.action)) return false;
  a.executionReady = r.u8() != 0;
  a.quarantined = r.u8() != 0;
  a.quarantineId = QuarantineId::read(r);
  a.inRecovery = r.u8() != 0;
  a.timestamp = Timestamp::read(r);
  a.healthGeneration = HealthGeneration::read(r);
  a.policyGeneration = PolicyGeneration::read(r);
  a.deviceGeneration = DeviceGeneration::read(r);
  const auto n = r.u32();
  if (r.fail() || n > util::kMaxCount) { r.set_fail(); return false; }
  a.reasons.reserve(n);
  for (std::uint32_t i = 0; i < n; ++i) a.reasons.push_back(r.string());
  a.digest = r.string();
  return r.ok();
}
}  // namespace

std::vector<std::uint8_t> encodeStateInternal(const HealthStore& store) {
  wire::Writer body;
  store.policy_.write(body);
  body.u64(store.policyGeneration_.get());
  body.u64(store.coordinatorEpoch_);
  body.u32(static_cast<std::uint32_t>(store.devices_.size()));
  for (const auto& [id, d] : store.devices_) {
    (void)id;
    d.id.write(body);
    d.uuid.write(body);
    d.node.write(body);
    d.lastWorker.write(body);
    d.lastWorkerBootId.write(body);
    d.generation.write(body);
    write(body, d.state);
    write(body, d.readiness);
    body.u64(d.healthGeneration.get());
    body.u64(d.transitionCount);
    body.u32(static_cast<std::uint32_t>(d.dimensions.size()));
    for (const auto& [kind, dim] : d.dimensions) { (void)kind; dim.write(body); }
    body.u32(static_cast<std::uint32_t>(d.recentEvidence.size()));
    for (const auto& e : d.recentEvidence) e.write(body);
    body.u32(static_cast<std::uint32_t>(d.faults.size()));
    for (const auto& f : d.faults.all()) f.write(body);
    body.u32(static_cast<std::uint32_t>(d.validations.size()));
    for (const auto& v : d.validations) v.write(body);
    body.u32(static_cast<std::uint32_t>(d.changes.size()));
    for (const auto& hc : d.changes.all()) hc.write(body);
    writeAssessment(body, d.latestAssessment);
    write(body, d.highestValidation);
    d.lastObservation.write(body);
    d.firstSeen.write(body);
    body.u8(d.quarantine ? 1 : 0);
    if (d.quarantine) d.quarantine->write(body);
    body.u8(d.recovery ? 1 : 0);
    if (d.recovery) d.recovery->write(body);
    body.u8(d.deviceGenerationChangedFlag ? 1 : 0);
    body.u8(d.policyGenerationChangedFlag ? 1 : 0);
    body.u8(d.inRecovery ? 1 : 0);
    body.u8(d.present ? 1 : 0);
  }

  wire::Writer frame;
  frame.bytes(kMagicP, 4);
  frame.u32(kPersistenceSchemaVersion);
  frame.u32(static_cast<std::uint32_t>(body.size()));
  frame.raw(body.data());
  std::vector<std::uint8_t> result = frame.data();
  std::uint32_t crc = util::crc32(result.data(), result.size());
  std::uint8_t crcBytes[4];
  util::store_le(crc, crcBytes);
  result.insert(result.end(), crcBytes, crcBytes + 4);
  return result;
}

bool decodeStateInternal(const std::vector<std::uint8_t>& data, HealthStore& out, std::string* error) {
  if (data.size() < 4 + 4 + 4 + 4) { if (error) *error = "file too short"; return false; }
  if (std::memcmp(data.data(), kMagicP, 4) != 0) { if (error) *error = "bad magic"; return false; }
  std::uint32_t version = util::load_le32(data.data() + 4);
  if (version != kPersistenceSchemaVersion) { if (error) *error = "unsupported schema version"; return false; }
  std::uint32_t payloadLen = util::load_le32(data.data() + 8);
  const std::size_t bodyLen = 4 + 4 + 4 + static_cast<std::size_t>(payloadLen);
  if (data.size() != bodyLen + 4) { if (error) *error = "truncated or trailing garbage"; return false; }
  const std::uint32_t crcExpected = util::load_le32(data.data() + bodyLen);
  const std::uint32_t crcActual = util::crc32(data.data(), bodyLen);
  if (crcExpected != crcActual) { if (error) *error = "crc mismatch"; return false; }

  wire::Reader r(std::span<const std::uint8_t>(data.data() + 12, payloadLen));
  HealthPolicy pol;
  if (!HealthPolicy::read(r, pol)) { if (error) *error = "invalid policy"; return false; }
  const PolicyGeneration policyGen(r.u64());
  const std::uint64_t epoch = r.u64();
  if (!r.ok() || policyGen.is_null()) { if (error) *error = "invalid policy generation"; return false; }

  out.policy_ = pol;
  out.policyGeneration_ = policyGen;
  out.coordinatorEpoch_ = epoch;
  out.devices_.clear();
  out.uuidIndex_.clear();
  out.stateIndex_.clear();
  out.readinessIndex_.clear();
  out.faultIndex_.clear();
  out.quarantineIndex_.clear();
  out.staleIndex_.clear();
  out.globalFaults_.clear();
  out.globalChanges_.clear();

  const std::uint32_t deviceCount = r.u32();
  if (r.fail() || deviceCount > util::kMaxCount) { if (error) *error = "invalid device count"; return false; }
  std::map<DeviceUuid, AcceleratorId> seenUuid;
  std::uint64_t maxDeviceRaw = 0;
  for (std::uint32_t i = 0; i < deviceCount; ++i) {
    HealthStore::Device d;
    d.id = AcceleratorId::read(r);
    d.uuid = DeviceUuid::read(r);
    d.node = NodeId::read(r);
    d.lastWorker = WorkerId::read(r);
    d.lastWorkerBootId = WorkerBootId::read(r);
    d.generation = DeviceGeneration::read(r);
    if (!read(r, d.state)) { if (error) *error = "invalid health state"; return false; }
    if (!read(r, d.readiness)) { if (error) *error = "invalid readiness"; return false; }
    d.healthGeneration = HealthGeneration(r.u64());
    d.transitionCount = r.u64();
    if (d.healthGeneration.is_null()) { if (error) *error = "invalid health generation"; return false; }
    const std::uint32_t dimCount = r.u32();
    if (r.fail() || dimCount > util::kMaxCount) { if (error) *error = "invalid dimension count"; return false; }
    for (std::uint32_t j = 0; j < dimCount; ++j) {
      HealthDimension dim;
      if (!HealthDimension::read(r, dim)) { if (error) *error = "invalid dimension"; return false; }
      d.dimensions[dim.kind] = dim;
    }
    const std::uint32_t evCount = r.u32();
    if (r.fail() || evCount > util::kMaxCount) { if (error) *error = "invalid evidence count"; return false; }
    for (std::uint32_t j = 0; j < evCount; ++j) {
      Evidence e;
      if (!Evidence::read(r, e)) { if (error) *error = "invalid evidence"; return false; }
      d.recentEvidence.push_back(std::move(e));
    }
    const std::uint32_t faultCount = r.u32();
    if (r.fail() || faultCount > util::kMaxCount) { if (error) *error = "invalid fault count"; return false; }
    std::set<FaultId> seenFault;
    for (std::uint32_t j = 0; j < faultCount; ++j) {
      Fault f;
      if (!Fault::read(r, f)) { if (error) *error = "invalid fault"; return false; }
      if (!seenFault.insert(f.id).second) { if (error) *error = "duplicate fault id"; return false; }
      d.faults.record(std::move(f));
    }
    const std::uint32_t valCount = r.u32();
    if (r.fail() || valCount > util::kMaxCount) { if (error) *error = "invalid validation count"; return false; }
    std::set<ValidationRunId> seenVal;
    for (std::uint32_t j = 0; j < valCount; ++j) {
      ValidationRecord v;
      if (!ValidationRecord::read(r, v)) { if (error) *error = "invalid validation"; return false; }
      if (!seenVal.insert(v.id).second) { if (error) *error = "duplicate validation id"; return false; }
      d.validations.push_back(std::move(v));
    }
    const std::uint32_t chCount = r.u32();
    if (r.fail() || chCount > util::kMaxCount) { if (error) *error = "invalid change count"; return false; }
    for (std::uint32_t j = 0; j < chCount; ++j) {
      HealthChange hc;
      if (!HealthChange::read(r, hc)) { if (error) *error = "invalid change"; return false; }
      d.changes.append(std::move(hc));
    }
    if (!readAssessment(r, d.latestAssessment)) { if (error) *error = "invalid assessment"; return false; }
    if (!read(r, d.highestValidation)) { if (error) *error = "invalid validation depth"; return false; }
    d.lastObservation = Timestamp::read(r);
    d.firstSeen = Timestamp::read(r);
    const std::uint8_t hasQuar = r.u8();
    if (hasQuar) {
      QuarantineRecord q;
      if (!QuarantineRecord::read(r, q)) { if (error) *error = "invalid quarantine"; return false; }
      d.quarantine = q;
    }
    const std::uint8_t hasRec = r.u8();
    if (hasRec) {
      RecoveryRecord rec;
      if (!RecoveryRecord::read(r, rec)) { if (error) *error = "invalid recovery"; return false; }
      d.recovery = rec;
    }
    d.deviceGenerationChangedFlag = r.u8() != 0;
    d.policyGenerationChangedFlag = r.u8() != 0;
    d.inRecovery = r.u8() != 0;
    d.present = r.u8() != 0;
    if (r.fail()) { if (error) *error = "malformed device record"; return false; }

    // Impossible-state rejection: an active quarantine with a HEALTHY state is invalid.
    if (d.quarantine && d.quarantine->active && d.state == HealthState::HEALTHY) {
      if (error) *error = "impossible state: active quarantine with HEALTHY"; return false;
    }
    if (seenUuid.count(d.uuid)) { if (error) *error = "duplicate device uuid"; return false; }
    seenUuid[d.uuid] = d.id;
    maxDeviceRaw = std::max(maxDeviceRaw, d.id.get());
    out.devices_[d.id] = std::move(d);
    out.uuidIndex_[out.devices_[d.id].uuid] = out.devices_[d.id].id;
  }

  if (r.remaining() != 0) { if (error) *error = "trailing bytes in payload"; return false; }

  out.nextDeviceRaw_ = maxDeviceRaw + 1;
  out.nextFaultRaw_ = out.nextDeviceRaw_;
  out.nextObservationRaw_ = out.nextDeviceRaw_;
  out.nextValidationRaw_ = out.nextDeviceRaw_;
  out.nextQuarantineRaw_ = out.nextDeviceRaw_;
  out.nextRecoveryRaw_ = out.nextDeviceRaw_;
  out.nextAssessmentRaw_ = out.nextDeviceRaw_;
  out.nextIncidentRaw_ = out.nextDeviceRaw_;
  out.recomputeIndexes();
  return true;
}

bool saveInternal(const std::string& path, const HealthStore& store, std::string* error) {
  const std::vector<std::uint8_t> bytes = encodeStateInternal(store);
  const std::string tmp = path + ".tmp";
  {
    std::ofstream os(tmp, std::ios::binary | std::ios::trunc);
    if (!os) { if (error) *error = "cannot open for write"; return false; }
    os.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    os.flush();
    if (!os) { if (error) *error = "write failed"; return false; }
  }
  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);
  if (ec) { std::filesystem::remove(tmp); if (error) *error = "atomic replace failed"; return false; }
  return true;
}

bool loadInternal(const std::string& path, HealthStore& out, std::string* error) {
  std::ifstream is(path, std::ios::binary);
  if (!is) { if (error) *error = "cannot open for read"; return false; }
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
  if (!decodeStateInternal(bytes, out, error)) return false;
  // A coordinator restart is a trust boundary: persisted dynamic evidence must
  // not be treated as verified CURRENT until refreshed by a live worker.
  for (auto& [id, d] : out.devices_) { (void)id; d.requiresRevalidation = true; }
  out.reassessAll();
  return true;
}

}  // namespace ah
