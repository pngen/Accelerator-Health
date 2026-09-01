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
#include "accelerator-health/Time.hpp"
#include <chrono>

namespace ah {
Timestamp SystemClock::now() const {
  using namespace std::chrono;
  const auto ns = duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
  return Timestamp{ns};
}
}  // namespace ah
