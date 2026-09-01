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
#include "accelerator-health/StateMachine.hpp"

#include <sstream>

namespace ah {

const std::vector<Transition>& legalTransitions() noexcept {
  static const std::vector<Transition> k = {
    {HealthState::UNKNOWN, HealthState::INITIALIZING, "init"},
    {HealthState::UNKNOWN, HealthState::OFFLINE, "offline"},
    {HealthState::UNKNOWN, HealthState::LOST, "lost"},
    {HealthState::UNKNOWN, HealthState::HEALTHY, "first-validated"},

    {HealthState::INITIALIZING, HealthState::HEALTHY, "init-ok"},
    {HealthState::INITIALIZING, HealthState::DEGRADED, "init-degraded"},
    {HealthState::INITIALIZING, HealthState::UNHEALTHY, "init-unhealthy"},
    {HealthState::INITIALIZING, HealthState::FAILED, "init-failed"},
    {HealthState::INITIALIZING, HealthState::DRAINING, "init-drain"},
    {HealthState::INITIALIZING, HealthState::QUARANTINED, "init-quarantine"},
    {HealthState::INITIALIZING, HealthState::OFFLINE, "init-offline"},
    {HealthState::INITIALIZING, HealthState::LOST, "init-lost"},

    {HealthState::HEALTHY, HealthState::DEGRADED, "degraded"},
    {HealthState::HEALTHY, HealthState::UNHEALTHY, "unhealthy"},
    {HealthState::HEALTHY, HealthState::FAILED, "failed"},
    {HealthState::HEALTHY, HealthState::DRAINING, "drain"},
    {HealthState::HEALTHY, HealthState::QUARANTINED, "quarantine"},
    {HealthState::HEALTHY, HealthState::OFFLINE, "offline"},
    {HealthState::HEALTHY, HealthState::LOST, "lost"},

    {HealthState::DEGRADED, HealthState::UNHEALTHY, "unhealthy"},
    {HealthState::DEGRADED, HealthState::FAILED, "failed"},
    {HealthState::DEGRADED, HealthState::DRAINING, "drain"},
    {HealthState::DEGRADED, HealthState::QUARANTINED, "quarantine"},
    {HealthState::DEGRADED, HealthState::REVALIDATING, "revalidate"},
    {HealthState::DEGRADED, HealthState::OFFLINE, "offline"},
    {HealthState::DEGRADED, HealthState::LOST, "lost"},

    {HealthState::UNHEALTHY, HealthState::DRAINING, "drain"},
    {HealthState::UNHEALTHY, HealthState::FAILED, "failed"},
    {HealthState::UNHEALTHY, HealthState::QUARANTINED, "quarantine"},
    {HealthState::UNHEALTHY, HealthState::OFFLINE, "offline"},
    {HealthState::UNHEALTHY, HealthState::LOST, "lost"},

    {HealthState::FAILED, HealthState::QUARANTINED, "quarantine"},
    {HealthState::FAILED, HealthState::DRAINING, "drain"},
    {HealthState::FAILED, HealthState::RECOVERING, "recover"},
    {HealthState::FAILED, HealthState::OFFLINE, "offline"},
    {HealthState::FAILED, HealthState::LOST, "lost"},

    {HealthState::DRAINING, HealthState::DRAINED, "drained"},
    {HealthState::DRAINING, HealthState::FAILED, "failed"},
    {HealthState::DRAINING, HealthState::QUARANTINED, "quarantine"},
    {HealthState::DRAINING, HealthState::OFFLINE, "offline"},
    {HealthState::DRAINING, HealthState::LOST, "lost"},

    {HealthState::DRAINED, HealthState::RECOVERING, "recover"},
    {HealthState::DRAINED, HealthState::REVALIDATING, "revalidate"},
    {HealthState::DRAINED, HealthState::OFFLINE, "offline"},
    {HealthState::DRAINED, HealthState::LOST, "lost"},

    {HealthState::QUARANTINED, HealthState::RECOVERING, "recover"},
    {HealthState::QUARANTINED, HealthState::OFFLINE, "offline"},
    {HealthState::QUARANTINED, HealthState::LOST, "lost"},

    {HealthState::RECOVERING, HealthState::REVALIDATING, "revalidate"},
    {HealthState::RECOVERING, HealthState::FAILED, "failed"},
    {HealthState::RECOVERING, HealthState::QUARANTINED, "quarantine"},
    {HealthState::RECOVERING, HealthState::OFFLINE, "offline"},
    {HealthState::RECOVERING, HealthState::LOST, "lost"},

    {HealthState::REVALIDATING, HealthState::HEALTHY, "revalidated-ok"},
    {HealthState::REVALIDATING, HealthState::DEGRADED, "revalidated-degraded"},
    {HealthState::REVALIDATING, HealthState::UNHEALTHY, "revalidated-unhealthy"},
    {HealthState::REVALIDATING, HealthState::FAILED, "revalidated-failed"},
    {HealthState::REVALIDATING, HealthState::QUARANTINED, "revalidated-quarantine"},
    {HealthState::REVALIDATING, HealthState::OFFLINE, "offline"},
    {HealthState::REVALIDATING, HealthState::LOST, "lost"},

    {HealthState::OFFLINE, HealthState::INITIALIZING, "re-enumerate"},
    {HealthState::OFFLINE, HealthState::HEALTHY, "re-enumerated-ok"},
    {HealthState::OFFLINE, HealthState::LOST, "lost"},

    {HealthState::LOST, HealthState::OFFLINE, "offline"},
    {HealthState::LOST, HealthState::INITIALIZING, "re-enumerate"},
  };
  return k;
}

bool canTransition(HealthState from, HealthState to) noexcept {
  for (const auto& t : legalTransitions()) {
    if (t.from == from && t.to == to) return true;
  }
  return false;
}

InvalidTransition::InvalidTransition(HealthState from, HealthState to)
    : std::runtime_error("illegal health-state transition"), from_(from), to_(to) {}

const std::vector<HealthState>& recoverySequenceFromUnhealthy() noexcept {
  static const std::vector<HealthState> seq = {
    HealthState::UNHEALTHY, HealthState::DRAINING, HealthState::DRAINED,
    HealthState::RECOVERING, HealthState::REVALIDATING, HealthState::HEALTHY};
  return seq;
}
const std::vector<HealthState>& recoverySequenceFromFailed() noexcept {
  static const std::vector<HealthState> seq = {
    HealthState::FAILED, HealthState::QUARANTINED, HealthState::RECOVERING,
    HealthState::REVALIDATING, HealthState::HEALTHY};
  return seq;
}

}  // namespace ah
