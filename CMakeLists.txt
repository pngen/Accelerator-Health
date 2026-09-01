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

cmake_minimum_required(VERSION 3.24)
project(AcceleratorHealth VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AH_ENABLE_CUDA "Enable the NVIDIA CUDA backend (sm_120 / RTX 5090)" ON)
option(AH_BUILD_TESTS "Build the Accelerator Health test suite" ON)
option(AH_BUILD_EXAMPLES "Build the Accelerator Health examples" ON)
option(AH_BUILD_BENCHMARKS "Build the Accelerator Health benchmarks" ON)
option(AH_BUILD_APPS "Build the CLI / worker / coordinator applications" ON)
option(AH_INSTALL "Generate install rules and package config" ON)

include(CTest)
include(GNUInstallDirs)
find_package(Threads REQUIRED)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
if(MSVC)
  function(ah_cxx_flags t)
    target_compile_options(${t} PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /EHsc)
  endfunction()
else()
  function(ah_cxx_flags t)
    target_compile_options(${t} PRIVATE -Wall -Wextra -Werror)
  endfunction()
endif()

if(AH_ENABLE_CUDA)
  include(CheckLanguage)
  check_language(CUDA)
  if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    set(AH_HAS_CUDA ON)
    set(CMAKE_CUDA_STANDARD 20)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    message(STATUS "Accelerator Health: CUDA enabled")
  else()
    message(WARNING "Accelerator Health: CUDA not found; building without the CUDA backend")
  endif()
endif()

add_subdirectory(src)
if(AH_BUILD_TESTS)
  add_subdirectory(tests)
endif()
if(AH_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
if(AH_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
if(AH_BUILD_APPS)
  add_subdirectory(apps)
endif()

if(AH_INSTALL)
  install(TARGETS accelerator_health EXPORT AcceleratorHealthTargets
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/accelerator-health
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(EXPORT AcceleratorHealthTargets
          FILE AcceleratorHealthTargets.cmake
          NAMESPACE AcceleratorHealth::
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AcceleratorHealth)
  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/AcceleratorHealthConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/AcceleratorHealthConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/AcceleratorHealthConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AcceleratorHealth)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/AcceleratorHealthConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/AcceleratorHealthConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AcceleratorHealth)
endif()
