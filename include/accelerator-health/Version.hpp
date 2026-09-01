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

#define AH_VERSION_MAJOR 1
#define AH_VERSION_MINOR 0
#define AH_VERSION_PATCH 0

#define AH_STRINGIFY2(x) #x
#define AH_STRINGIFY(x) AH_STRINGIFY2(x)
#define AH_VERSION_STRING AH_STRINGIFY(AH_VERSION_MAJOR) "." AH_STRINGIFY(AH_VERSION_MINOR) "." AH_STRINGIFY(AH_VERSION_PATCH)

namespace ah::version {
inline constexpr int major = AH_VERSION_MAJOR;
inline constexpr int minor = AH_VERSION_MINOR;
inline constexpr int patch = AH_VERSION_PATCH;
inline constexpr const char* string() { return AH_VERSION_STRING; }
}  // namespace ah::version
