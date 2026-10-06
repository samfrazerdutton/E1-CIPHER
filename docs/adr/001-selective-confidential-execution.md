# ADR-001: Selective Confidential Execution

## Status

Accepted. Implemented in `include/e1cipher/runtime/` and measured in
`e1cipher experiment selective`.

## Context

A physical-AI workload mixes data at very different sensitivity levels
(a raw camera frame, a GPS fix, a vibration feature, a battery level) and
very different sizes. Three naive policies were considered:

## Why encrypting everything fails

Measured (`e1cipher experiment selective`, Mode B): CKKS ciphertext
expansion is **2,733x** over a 6-scalar plaintext feature vector at
N4096, and the measured encrypt+add latency for one feature vector is
~1.8ms — not free. Applying that uniformly to every operation in the
industrial-inspection workload (including low-sensitivity diagnostics)
produces 655,925 bytes and 8.8ms for a workload that has almost nothing
confidential in most of its fields. Worse: it does not even have a
sensible answer for a raw camera frame — this reference CKKS backend
operates on fixed-size feature vectors, not arbitrary unstructured
sensor streams, so "encrypt everything" either requires chunking raw
sensor data into ciphertexts (not implemented, likely still far too
expensive) or silently exempting it (which is what selective execution
does on purpose, explicitly, instead of by omission).

## Why local-only execution fails

Keeping everything on-device defeats the actual purpose of several
operations. `fleet_anomaly_statistic` and `vibration_feature` are
additive aggregates — their value exists only once combined across many
devices. A policy that never transmits anything cannot produce fleet-wide
intelligence at all, which is the point of the system.

## Why remote plaintext fails

Measured (Mode A): sending everything in the clear is cheap (184 bytes,
0.15ms) but exposes 160 bytes of CONFIDENTIAL data to whoever operates
the aggregator — exactly threat T1/T2 in `docs/threat-model.md`. Fast,
unacceptable.

## Decision

Classify every operation by sensitivity (`DataSensitivity`) and shape
(`additive`, `linear`), then route it through exactly one execution
backend (`runtime::Placement`): `Local` for SECRET and (by default)
RESTRICTED data; `Remote` plaintext only for PUBLIC data within budget;
`Encrypted`/`Accelerated` for CONFIDENTIAL+ data, chosen deterministically
and explainably by `runtime::Runtime::decide`. Fail closed (`Blocked`) if
confidentiality is required and no crypto backend exists.

## Consequences

Measured (Mode C): zero security exposure (matching Mode B) at 60% of
Mode B's bandwidth and faster latency (1.77ms vs 8.80ms), because
low-sensitivity operations skip encryption entirely rather than paying
its cost needlessly. This is the actual thesis of the project — not that
CKKS is cheap (it measurably is not), but that *not* using it where it
isn't needed is where the real savings come from.
