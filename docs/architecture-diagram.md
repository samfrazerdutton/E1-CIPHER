# Architecture

## Application → runtime → placement → execution

```
APPLICATION  (apps/drone_inspection, apps/fleet_gateway, apps/runtime_demo,
              apps/experiments -- three workload profiles, ONE runtime)
    |
    v
RUNTIME                     (runtime::Runtime)
    |
    v
POLICY / SECURITY            (runtime::SecurityPolicy, DataSensitivity)
    |
    v
COST MODEL + PLACEMENT ENGINE (runtime::CostModel, runtime::Runtime::decide)
    |
    v
+------------+--------------+----------------+-------------+
|   LOCAL    |   ENCRYPTED  |   ACCELERATED  |   REMOTE    |
| (host CPU, | (crypto::    | (runtime::     | (plaintext, |
|  no        |  CkksBackend,|  HostExecution |  low-       |
|  transmit) |  real SEAL)  |  Backend, real |  sensitivity|
|            |              |  NTT kernel)   |  only)      |
+------------+--------------+----------------+-------------+
                    |
                    v
             SECURE TELEMETRY  (lib-era: lib/diagnostics/audit;
                                 current: include/e1cipher/diagnostics/audit.hpp)
                    |
                    v
             FLEET AGGREGATION  (apps/fleet_gateway -- gateway role
                                 structurally never holds a SecretKey)
```

`BLOCKED` is a fifth, real `runtime::Placement` value — reached when
confidentiality is required but no crypto backend is available. It is
not drawn as a fork in the diagram above because it's a terminal,
fail-closed state, not a normal execution path; see
`docs/adr/001-selective-confidential-execution.md` and
`tests/security/test_fail_closed.cpp`.

## The CKKS path (what happens inside ENCRYPTED/ACCELERATED)

```
PLAINTEXT FEATURE VECTOR
    |
    v
ENCODE               (slot domain -> coefficient domain)
    |
    v
ENCRYPT              (RLWE sample + add)
    |
    v
RNS DECOMPOSITION    (split across coeff_modulus_bits limbs)
    |
    v
NTT                  (forward, per limb -- O(N log N) butterfly network)
    |                 <-- runtime::KernelId::Ntt, HostExecutionBackend
    v
MODULAR MULTIPLY-ADD (against the public key-switch matrix)
    |
    v
INVERSE NTT
    |
    v
RNS RECOMPOSITION
    |
    v
CIPHERTEXT OUT       (measured: 131,185 bytes @ N4096 -- 2,733x the
                       plaintext feature vector)
```

Measured, per-stage, at N8192 (`e1cipher experiment accelerator`):
relinearize 20.8%, rescale 25.6%, rotate 18.3% of total op latency —
**64.6% combined** — all three built from exactly the NTT →
modular-multiply-add → inverse-NTT shape drawn above. That combined
figure, not any single operation in isolation, is why
`docs/adr/002-ntt-as-accelerator-candidate.md` identifies the NTT/
key-switching family specifically as the architecturally interesting
part of this workload — see that ADR and
`docs/e1-hardware-validation-plan.md` for what would actually need to be
measured on E1 to test the idea, and `docs/technical-deep-dive.md` for
why each stage above costs what it does.
