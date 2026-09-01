# Copyright 2026 Summon Software Labs
# Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.

add_executable(ah-coordinator ah-coordinator.cpp)
target_link_libraries(ah-coordinator PRIVATE AcceleratorHealth::AcceleratorHealth)
ah_cxx_flags(ah-coordinator)

add_executable(ah-worker ah-worker.cpp)
target_link_libraries(ah-worker PRIVATE AcceleratorHealth::AcceleratorHealth)
ah_cxx_flags(ah-worker)

add_executable(ahc ahc.cpp)
target_link_libraries(ahc PRIVATE AcceleratorHealth::AcceleratorHealth)
ah_cxx_flags(ahc)
if(AH_HAS_CUDA)
  target_link_libraries(ahc PRIVATE AcceleratorHealth::Cuda)
  target_compile_definitions(ahc PRIVATE AH_HAS_CUDA=1)
endif()
