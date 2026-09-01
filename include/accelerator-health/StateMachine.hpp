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

#include <stdexcept>
#include <string>
#include <vector>

namespace ah {

// Explicit named health state machine. Only transitions declared legal here may
// occur; every other transition fails deterministically and throws. The key
// invariant: HEALTHY can only be reached from INITIALIZING, OFFLINE or
// REVALIDATING. No single successful probe can bypass recovery/revalidation.

struct Transition {
  HealthState from;
  HealthState to;
  std::string reason;
};

const std::vector<Transition>& legalTransitions() noexcept;

bool canTransition(HealthState from, HealthState to) noexcept;

class InvalidTransition : public std::runtime_error {
 public:
  InvalidTransition(HealthState from, HealthState to);
  HealthState from() const noexcept { return from_; }
  HealthState to() const noexcept { return to_; }

 private:
  HealthState from_;
  HealthState to_;
};

class StateMachine {
 public:
  explicit StateMachine(HealthState initial = HealthState::UNKNOWN) : state_(initial) {}
  HealthState state() const noexcept { return state_; }

  void transitionTo(HealthState to) {
    if (!canTransition(state_, to)) throw InvalidTransition(state_, to);
    state_ = to;
    ++generation_;
  }
  std::uint64_t transitionCount() const noexcept { return generation_; }

 private:
  HealthState state_;
  std::uint64_t generation_ = 0;
};

const std::vector<HealthState>& recoverySequenceFromUnhealthy() noexcept;
const std::vector<HealthState>& recoverySequenceFromFailed() noexcept;

}  // namespace ah
