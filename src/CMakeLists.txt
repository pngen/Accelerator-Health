# Copyright 2026 Summon Software Labs
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#     http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

add_library(accelerator_health STATIC
  Clock.cpp
  StateMachine.cpp
  Policy.cpp
  Readiness.cpp
  Assessment.cpp
  History.cpp
  Store.cpp
  Persistence.cpp
  Backend.cpp
  FakeBackend.cpp
  Protocol.cpp
  Net.cpp
  Worker.cpp
  Coordinator.cpp
)
add_library(AcceleratorHealth::AcceleratorHealth ALIAS accelerator_health)
target_include_directories(accelerator_health PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(accelerator_health PUBLIC Threads::Threads)
ah_cxx_flags(accelerator_health)
if(WIN32)
  target_link_libraries(accelerator_health PUBLIC ws2_32)
endif()
target_compile_features(accelerator_health PUBLIC cxx_std_20)
if(AH_HAS_CUDA)
  add_subdirectory(cuda)
endif()
