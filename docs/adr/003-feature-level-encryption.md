# ADR-003: Feature-Level Encryption Instead of Raw Sensor Encryption

## Status

Accepted. Implemented in `src/fusion/features.cpp` (unchanged from the
prior session; reused by the new runtime) and `runtime::Operation`'s
`vector_length`/`plaintext_bytes` split.

## Context

A raw camera frame in `workload::industrial_inspection_workload()` is
modeled at 1920×1080 = 2,073,600 bytes. This project's CKKS backend
encrypts fixed-size feature vectors (6 scalars, 48 bytes) into a
131,185-byte ciphertext — a **2,733x** expansion. Applying that same
expansion ratio to a 2MB camera frame would produce a theoretical
multi-gigabyte payload per frame, per device, per tick. That is not a
rounding error; it is a different order of magnitude of infeasibility.

## Decision

Raw, high-bandwidth sensor streams are never candidates for encryption
in this architecture. They are reduced to compact, derived features
*locally* (sensor fusion, unencrypted, on-device) before any placement
decision is made. Only the derived, compact representation is ever
eligible for `Placement::Encrypted`/`Placement::Accelerated`.

## Consequences

- Bandwidth and latency implications are the entire point: a 48-byte
  feature vector's ciphertext (131,185 bytes) is itself already a 2,733x
  expansion — doing the same to 2MB of raw imagery is categorically
  worse, not just proportionally worse, because this reference CKKS
  backend has no chunking/streaming ciphertext story for arbitrary-sized
  inputs.
- This is also a security property, not just a performance one: raw
  imagery is the single most re-identifying, highest-value data a drone
  produces (`docs/threat-model.md`'s LOCAL_ONLY classification for
  `camera_frame`). Keeping it unencrypted-but-local is stricter than
  keeping it encrypted-and-transmitted would be — it never has a network
  exposure window at all.
- `runtime::CostModel`'s calibration is deliberately scoped to
  feature-vector-sized operations (see `cost_model.hpp`'s class comment)
  rather than generalized to arbitrary byte counts — this ADR is why:
  there is no sane "encrypted cost" to model for raw sensor data in this
  architecture, because the architecture never attempts it.
