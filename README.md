# Accelerator Health

Accelerator Health is a C++20 systems runtime that answers a single, authoritative
question for a compute accelerator:

> Is this accelerator actually healthy enough to execute work now, what evidence
> supports that judgment, what is degraded or failing, how confident is the
> diagnosis, what changed, and when must the device be drained, quarantined,
> revalidated, or returned to service?

It is **not** a monitoring dashboard, an NVML wrapper, a telemetry exporter, a
generic watchdog, an alerting service, a synthetic benchmark harness, or a simple
HEALTHY/UNHEALTHY flag. It owns the *interpretation* of accelerator operational
health: it consumes raw observations and produces authoritative, evidence-based
health judgments. Consumers (GPU fleet agents, admission, placement, scheduling,
draining, quarantine, failover, replica management, resource brokerage, recovery,
lifecycle governance) may call into Accelerator Health, but Accelerator Health
does not schedule, maintain a fleet registry, broker resources, or run an
observability stack.

## Core systems question

See the opening question. The runtime treats a device as healthy only on the
basis of explicit, fresh evidence, failure history, validation depth, and bounded
policy. A device is not HEALTHY merely because it enumerates successfully.

## Systems boundary

- **Owns:** interpretation of health state, readiness, diagnosis, change history,
  quarantine/recovery/revalidation policy.
- **Consumes:** enumeration, driver/runtime status, capability, memory behavior,
  CUDA init, allocation, transfer, kernel execution, synchronization, numerical
  verification, ECC/error info, thermal, power, link/interconnect, process and
  runtime failures, reset/restart events, operator actions, historical state.
- **Does not own:** scheduling, fleet registry, resource brokerage, or
  observability.

## Health model

Health is a typed state machine over: UNKNOWN, INITIALIZING, HEALTHY, DEGRADED,
UNHEALTHY, FAILED, DRAINING, DRAINED, QUARANTINED, RECOVERING, REVALIDATING,
OFFLINE, LOST. Only explicitly declared transitions are legal; any other
transition fails deterministically. The invariant is enforced such that HEALTHY
can only be reached from INITIALIZING, OFFLINE, or REVALIDATING, so no single
successful probe can bypass recovery/revalidation.

Health is decomposed into independent *dimensions* (enumeration, driver, runtime
initialization, allocation, transfer, execution, synchronization, numerical
correctness, memory integrity, memory pressure, temperature, power, interconnect,
fatal-error, restart/reset, freshness, validation confidence). Each dimension
carries current state, evidence source, timestamp, observation generation,
confidence, severity, a reason, and an optional measurement.

Severity is a separate axis (INFO, WARNING, DEGRADED, CRITICAL, FATAL) and never
collapses into a single opaque score. A fatal condition is never silently
downgraded by an unrelated successful probe.

## Evidence model

Evidence is classified as MEASURED, REPORTED, DERIVED, RECONSTRUCTED, INFERRED,
or UNKNOWN. Evidence provenance is tracked; inferred or reconstructed evidence is
never silently promoted to measured fact. Prior failures are never rewritten by
later successful observations.

## Validation depth

Validation depth is ordered: NONE, ENUMERATION_ONLY, INITIALIZATION, ALLOCATION,
TRANSFER, EXECUTION, NUMERICAL, FULL. A device that has only been enumerated is
never described as fully validated; the highest completed level is exposed.

## Readiness

Execution readiness is a separate axis from health. A HEALTHY device may still be
NOT_READY when evidence is stale, required validation is incomplete, the policy or
device generation changed, runtime compatibility changed, or the device is
recovering. Readiness states: READY, READY_DEGRADED, NOT_READY,
REVALIDATION_REQUIRED, DRAIN_REQUIRED, QUARANTINED, UNKNOWN. Every decision is
explained.

## Fault model

Typed faults (ENUMERATION_FAILURE, DRIVER_FAILURE, CUDA_INIT_FAILURE,
ALLOCATION_FAILURE, TRANSFER_FAILURE, KERNEL_LAUNCH_FAILURE,
SYNCHRONIZATION_FAILURE, NUMERICAL_MISMATCH, MEMORY_INTEGRITY_FAILURE,
OUT_OF_MEMORY, DEVICE_LOST, DEVICE_RESET, FATAL_RUNTIME_ERROR,
THERMAL_DEGRADATION, POWER_DEGRADATION, INTERCONNECT_DEGRADATION, STALE_EVIDENCE,
IDENTITY_CHANGE, UNKNOWN_FAULT) carry fault id, device, type, severity, timestamp,
observation generation, worker/process incarnation, evidence, transient /
recoverable / fatal classification, causal metadata, and resolution state.
Append-only fault history preserves first/latest occurrence, repeated and
consecutive counts, recovery events, recurrence, and clustering. Prior failures
are never erased after recovery.

## Hysteresis

Policy-driven hysteresis prevents health flapping: N failures before DEGRADED,
M failures before UNHEALTHY, a quarantine threshold, a required successful
validation sequence before recovery, and separate treatment for fatal faults.
Time-driven behavior is injected through deterministic clocks; tests never sleep.

## Quarantine

Quarantine is a first-class state triggered by fatal faults, repeated critical
faults, numerical mismatch, memory corruption evidence, identity inconsistency,
operator action, failed recovery, or policy rule. It forces readiness NOT_READY,
preserves history, requires an explicit or policy-approved recovery path, and
cannot be silently cleared by an unrelated successful probe.

## Recovery and revalidation

Recovery flows are explicit and evidence-based. Representative sequences:
UNHEALTHY → DRAINING → DRAINED → RECOVERING → REVALIDATING → HEALTHY, and
FAILED → QUARANTINED → RECOVERING → REVALIDATING → HEALTHY. Revalidation
profiles (BASIC, MEMORY, TRANSFER, EXECUTION, NUMERICAL, FULL) run the documented
sequence on supported NVIDIA/CUDA systems and produce explicit evidence records.
A device does not become HEALTHY purely because a process restarted.

## Freshness

Every dynamic observation carries a timestamp, ObservationGeneration, source,
WorkerBootId, device generation, and a freshness status (CURRENT, AGING, STALE,
EXPIRED). Old healthy evidence does not remain authoritative forever; stale
evidence reduces confidence and forces revalidation according to policy.

## Distributed authority and fencing

Distributed health reports are fenced by CoordinatorEpoch, WorkerBootId,
ObservationGeneration, HealthGeneration, ValidationGeneration, DeviceGeneration,
and PolicyGeneration. Stale reports are rejected deterministically. A restarted
worker with a fresh boot id cannot overwrite current health with reports from a
prior process incarnation. A real coordinator plus worker OS processes over
framed TCP exercise this end-to-end.

## Persistence and recovery

Deterministic, versioned binary persistence stores device identity, health and
readiness state, fault history, quarantine/recovery state, validation records,
generations, policy generation, and historical transitions. The format has an
explicit schema version, fixed byte order, CRC-32 integrity, bounded lengths, and
atomic durable replacement. Corruption, truncation, trailing garbage, duplicate
IDs, invalid enums, impossible states, and invalid generations are rejected.
After recovery, historical records remain and dynamic evidence is never silently
treated as CURRENT until refreshed by a live worker.

## RTX 5090 validation

The CUDA backend performs a real full validation on the NVIDIA GeForce RTX 5090
(compute capability 12.0 / sm_120): device enumeration, identity confirmation,
CUDA initialization, bounded allocation, host-to-device transfer, real CUDA
kernel execution, synchronization, device-to-host transfer, CPU-reference
comparison, cleanup, and a memory recovery check. It publishes a FULL validation
assessment and returns READY only after success. Controlled negative paths
(forced numerical mismatch and forced validation failure) are exercised without
touching physical hardware.

## CLI and examples

`ahc` provides list, inspect, health, readiness, faults, history, validate,
quarantine, clear-quarantine, begin-recovery, revalidate, explain, changes,
snapshot, save, recover, and benchmark operations with JSON output where useful.
See `examples/` for 14 substantial runnable examples (basic health, validation
progression, transient degradation, hysteresis, fatal fault, quarantine,
recovery/revalidation, stale evidence, policy generation, fault history,
deterministic explanation, two-worker store, persistence/recovery, and the RTX
5090 full validation). `ah-coordinator` and `ah-worker` are the distributed
processes used by the multiprocess proof.

## Build, install, use

Requires CMake 3.24+, a C++20 compiler (MSVC on Windows), and optionally CUDA 13.1
plus a real NVIDIA GPU (target architecture sm_120). NVML is optional; the core
CUDA proof operates without it.

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=120
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix <prefix>
```

Downstream consumers:

```cmake
find_package(AcceleratorHealth CONFIG REQUIRED)
target_link_libraries(myapp PRIVATE AcceleratorHealth::AcceleratorHealth)
```

## Benchmark summary

Bounded micro-benchmarks are provided for observation ingestion, health
assessment (snapshot), indexed lookup, fault recording, protocol encode/decode,
and persistence encoding, reporting per-op microsecond costs. See
`benchmarks/` (run `ah-bench`).

## Materially relevant limitations

- Health is interpreted from the evidence that reaches the runtime. Unsupported
  observations (e.g., thermal, power, interconnect without NVML) are reported as
  unavailable, never as healthy evidence.
- The CUDA backend is validated on a single NVIDIA GeForce RTX 5090
  (compute capability 12.0 / sm_120). Physical multi-GPU health validation is not
  claimed.
- Synthetic fault/device scenarios are clearly labeled synthetic; no fabricated
  fault evidence, health state, benchmark numbers, hardware results, or
  unsupported telemetry are presented.
- Free memory baselines are compared within a justified bounded delta, as CUDA
  memory pools may retain allocations; a leak beyond the delta fails validation.
- Accelerator Health does not transmit telemetry. Network egress is limited to
  the explicitly enabled coordinator/worker control channel documented here.

## License

Apache License 2.0. Copyright 2026 Summon Software Labs. No telemetry transmission.
