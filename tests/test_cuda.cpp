// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License"); see LICENSE.
#include "accelerator-health/CudaBackend.hpp"
#include "test_fw.hpp"
#include <string>
using namespace ah;

AH_TEST(rtx5090_full_validation) {
  CudaBackend be;
  CHECK_MSG(be.isCudaRuntimeAvailable(), "CUDA runtime must be available");
  auto devs = be.enumerate();
  CHECK_MSG(!devs.empty(), "at least one CUDA device");
  auto d0 = devs[0];
  CHECK(d0.name.find("5090") != std::string::npos);
  CHECK_EQ(d0.computeCapabilityMajor, 12);
  CHECK_EQ(d0.computeCapabilityMinor, 0);
  auto v = be.runValidation(d0.id, ValidationProfile::FULL);
  CHECK_MSG(v.passed, v.error.c_str());
  CHECK_EQ((int)v.depth, (int)ValidationDepth::FULL);
  CHECK(v.maxError <= 1e-6f);
  CHECK(v.freeAfter + (16 * 1024 * 1024) >= v.freeBefore);
}

AH_TEST(controlled_negative_numerical) {
  CudaBackend be;
  auto devs = be.enumerate();
  CHECK_MSG(!devs.empty(), "at least one CUDA device");
  be.setForceNumericalMismatch(true, 0.5f);
  auto v = be.runValidation(devs[0].id, ValidationProfile::FULL);
  CHECK_MSG(!v.passed, "forced numerical mismatch must fail validation");
}

AH_TEST(controlled_negative_failure) {
  CudaBackend be;
  auto devs = be.enumerate();
  CHECK_MSG(!devs.empty(), "at least one CUDA device");
  be.setForceValidationFailure(true, "synthetic validation failure");
  auto v = be.runValidation(devs[0].id, ValidationProfile::FULL);
  CHECK_MSG(!v.passed, "forced validation failure must fail");
}

int main(int argc, char** argv) { (void)argc; (void)argv; return testfw::runAll("test_cuda"); }
