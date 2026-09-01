# Copyright 2026 Summon Software Labs
# Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.

find_package(CUDAToolkit REQUIRED)

add_library(accelerator_health_cuda STATIC
  CudaBackend.cpp
  numeric_kernel.cu
)
add_library(AcceleratorHealth::Cuda ALIAS accelerator_health_cuda)
target_link_libraries(accelerator_health_cuda PUBLIC accelerator_health CUDA::cudart)
target_include_directories(accelerator_health_cuda PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(accelerator_health_cuda PROPERTIES CUDA_ARCHITECTURES 120)
if(MSVC)
  set_source_files_properties(numeric_kernel.cu PROPERTIES COMPILE_OPTIONS "-Xcompiler=/W4;-Xcompiler=/WX")
  set_source_files_properties(CudaBackend.cpp PROPERTIES COMPILE_OPTIONS "/W4;/WX;/permissive-;/Zc:__cplusplus;/EHsc")
endif()
