# Contributing to Accelerator Health

Thank you for your interest in contributing to Accelerator Health. This
document describes the contribution terms for this project.

## License

By contributing to this project, you agree that your contributions are
licensed under the **Apache License, Version 2.0**. See the `LICENSE`
file for the full license text and the `NOTICE` file for attribution
and license notices. There is **no separate Contributor License
Agreement (CLA)** requirement: you retain ownership of your
contributions and grant the project a license to use them under the
terms of the Apache License 2.0.

## License headers

New source files should carry the following header:

```
// Copyright 2026 Summon Software Labs
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
```

## Coding standards

- C++20, CMake, and MSVC on Windows (CUDA 13.1 for GPU backends).
- Build cleanly with `/W4` and `/WX`; no compiler warnings.
- The determinism of health judgments is a core invariant. Adversarial
  tests, fixed-seed property tests, and reproducible digests are expected.
- Time-driven behavior must be driven by injectable/deterministic clocks.
- Do not introduce opaque scoring; judgments must be inspectable.

## Architecture boundaries

- Accelerator Health owns health *interpretation*, not scheduling,
  fleet registry, resource brokerage, or observability.
- Keep the CUDA/NVIDIA backend behind the `Backend` abstraction.
- Do not make NVML a hard requirement.

## Testing

Run the full deterministic, concurrency, adversarial, persistence,
protocol, multiprocess, and hardware-backed suites before submitting.

## Pull requests

Please keep changes focused, add tests for new behavior, and ensure the
repository builds and tests cleanly in both Release and Debug.
