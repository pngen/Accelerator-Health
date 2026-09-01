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
#include "accelerator-health/History.hpp"

#include <algorithm>
#include <stdexcept>

namespace ah {

void FaultHistory::record(const Fault& f) {
  for (const auto& existing : faults_) {
    if (existing.id == f.id) throw std::invalid_argument("duplicate fault id");
  }
  faults_.push_back(f);
}
void FaultHistory::record(Fault&& f) {
  for (const auto& existing : faults_) {
    if (existing.id == f.id) throw std::invalid_argument("duplicate fault id");
  }
  faults_.push_back(std::move(f));
}

bool FaultHistory::resolve(FaultId id, Timestamp at) {
  for (auto& f : faults_) {
    if (f.id == id) {
      if (f.resolution != ResolutionState::OPEN) return false;  // no double resolution
      f.resolution = ResolutionState::RESOLVED;
      f.resolvedAt = at;
      return true;
    }
  }
  return false;
}

std::vector<Fault> FaultHistory::openFaults() const {
  std::vector<Fault> out;
  for (const auto& f : faults_) if (f.resolution == ResolutionState::OPEN) out.push_back(f);
  return out;
}

std::vector<Fault> FaultHistory::faultsOfType(FaultType t) const {
  std::vector<Fault> out;
  for (const auto& f : faults_) if (f.type == t) out.push_back(f);
  return out;
}

std::vector<Fault> FaultHistory::recent(std::size_t limit) const {
  if (limit >= faults_.size()) return faults_;
  return std::vector<Fault>(faults_.end() - static_cast<std::ptrdiff_t>(limit), faults_.end());
}

std::uint32_t FaultHistory::countOfType(FaultType t) const {
  std::uint32_t n = 0;
  for (const auto& f : faults_) if (f.type == t) ++n;
  return n;
}

std::uint32_t FaultHistory::consecutiveOfType(FaultType t) const {
  std::uint32_t n = 0;
  for (auto it = faults_.rbegin(); it != faults_.rend(); ++it) {
    if (it->type == t) ++n;
    else break;
  }
  return n;
}

Timestamp FaultHistory::firstOfType(FaultType t) const {
  for (const auto& f : faults_) if (f.type == t) return f.timestamp;
  return {};
}
Timestamp FaultHistory::latestOfType(FaultType t) const {
  Timestamp latest{};
  for (const auto& f : faults_) if (f.type == t && f.timestamp > latest) latest = f.timestamp;
  return latest;
}

bool FaultHistory::isFaultOpen(FaultId id) const {
  for (const auto& f : faults_) if (f.id == id) return f.resolution == ResolutionState::OPEN;
  return false;
}

DeviceFaultStats FaultHistory::stats() const {
  DeviceFaultStats s;
  s.total = static_cast<std::uint32_t>(faults_.size());
  for (const auto& f : faults_) {
    if (f.resolution == ResolutionState::OPEN) {
      if (f.fatal || f.severity == Severity::FATAL) s.hasOpenFatal = true;
      if (static_cast<int>(f.severity) >= static_cast<int>(Severity::CRITICAL)) {
        s.hasOpenCritical = true;
        ++s.criticalFatalOpen;
      }
    }
  }
  if (!faults_.empty()) {
    FaultType last = faults_.back().type;
    s.consecutiveLatest = consecutiveOfType(last);
  }
  return s;
}

void FaultHistory::write(wire::Writer& w) const {
  w.u32(static_cast<std::uint32_t>(faults_.size()));
  for (const auto& f : faults_) f.write(w);
}
bool FaultHistory::read(wire::Reader& r, FaultHistory& h) {
  const auto n = r.u32();
  if (r.fail() || n > util::kMaxCount) { r.set_fail(); return false; }
  h.faults_.clear();
  h.faults_.reserve(n);
  for (std::uint32_t i = 0; i < n; ++i) {
    Fault f;
    if (!Fault::read(r, f)) return false;
    if (h.isFaultOpen(f.id)) { r.set_fail(); return false; }  // duplicate id
    for (const auto& ex : h.faults_) if (ex.id == f.id) { r.set_fail(); return false; }
    h.faults_.push_back(std::move(f));
  }
  return r.ok();
}

}  // namespace ah
