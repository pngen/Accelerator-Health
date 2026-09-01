# Copyright 2026 Summon Software Labs
# Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.

function(ah_add_example exname)
  add_executable(${exname} ${exname}.cpp)
  target_link_libraries(${exname} PRIVATE AcceleratorHealth::AcceleratorHealth)
  ah_cxx_flags(${exname})
endfunction()

set(AH_EXAMPLES ex01_basic_health ex02_validation_depth ex03_transient_degradation ex04_failure_hysteresis ex05_fatal_fault ex06_quarantine ex07_recovery_revalidation ex08_stale_evidence ex09_policy_generation ex10_fault_history ex11_deterministic_explanation ex12_two_worker_store ex13_persistence_recovery ex14_rtx5090_full_validation)
foreach(ex ${AH_EXAMPLES})
  ah_add_example(${ex})
endforeach()
if(AH_HAS_CUDA)
  target_link_libraries(ex14_rtx5090_full_validation PRIVATE AcceleratorHealth::Cuda)
  target_compile_definitions(ex14_rtx5090_full_validation PRIVATE AH_HAS_CUDA=1)
endif()
