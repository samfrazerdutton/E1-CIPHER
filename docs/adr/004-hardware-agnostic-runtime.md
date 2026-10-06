# ADR-004: Hardware-Agnostic Runtime

## Status

Accepted. Implemented in `include/e1cipher/runtime/execution_backend.hpp`.

## Context

This project's stated purpose is to be "waiting for the hardware" (see
README), not something that needs rewriting when E1 access arrives. That
requires the application layer (`apps/`, `runtime::Workload`,
`runtime::Operation`) to never call into a specific accelerator's API
directly.

## Decision

`runtime::ExecutionBackend` is the only interface application code
depends on for accelerated execution: `allocate_buffer`,
`copy_to_device`, `copy_from_device`, `submit_kernel`, `synchronize`,
`query_capabilities`. `HostExecutionBackend` implements it today, in
real, measurable, host-reference terms. `E1ExecutionBackend` implements
the same interface and fails loudly (`std::runtime_error`, never a
silent host fallback) because no E1 hardware or toolchain exists in this
environment — see `cmake/FindE1Toolchain.cmake` for the same discipline
applied at build-configure time.

## Consequences

- Swapping in a real E1 backend means implementing
  `runtime::ExecutionBackend`'s six methods against the effcc
  toolchain/hardware and changing one line in `runtime::Runtime`'s
  constructor (which backend to construct) — not touching
  `runtime::Workload`, `runtime::Operation`, the policy logic in
  `runtime::Runtime::decide`, or any application in `apps/`.
- The cost model (`runtime::CostModel::estimate_accelerated`) already
  measures the *candidate kernel's* shape on host
  (`crypto::kernels::ntt`) through this same interface
  (`HostExecutionBackend::submit_kernel`) — so the moment a real
  `E1ExecutionBackend::submit_kernel` exists, the identical call site in
  `CostModel` starts producing `ResultClass::RealHardware` numbers
  instead of `ResultClass::HostReference` ones, with no other code
  change required.
- This is also why `KernelId` deliberately does not include
  `RnsDecompose`/`KeySwitchInner` as *implemented* host kernels (see
  `execution_backend.cpp`) — SEAL performs those internally, and listing
  them as "supported" would be describing a capability this reference
  implementation does not actually have, which this project's "never
  fabricate" rule forbids regardless of which hardware is involved.
