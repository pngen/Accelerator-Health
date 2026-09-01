// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
// CUDA kernel and host launcher used by Accelerator Health's full validation.
#include <cuda_runtime.h>

// Elementwise y = x * 2.0f. IEEE single-precision multiply is exact, so the
// CPU-reference comparison yields zero error for a correct kernel.
__global__ void ah_kernel_mult2(const float* __restrict__ in, float* __restrict__ out, int n) {
  const int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) {
    const float v = in[i];
    out[i] = v * 2.0f;
  }
}

extern "C" int ah_cuda_launch_mult2(const float* in, float* out, int n) {
  if (n <= 0) return 0;
  const int block = 256;
  const int grid = (n + block - 1) / block;
  ah_kernel_mult2<<<grid, block>>>(in, out, n);
  return static_cast<int>(cudaGetLastError());
}
