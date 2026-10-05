# Threat Model

This is a research prototype's threat register, not a certified security
assessment. It states what the architecture in this repository is designed
to resist, what it explicitly does not address, and why — per the project
rule to never claim protection the prototype does not actually provide.

## Scope

In scope: the confidential-compute pipeline in `lib/ckks`, `lib/policy`,
and `apps/fleet` — i.e., the path from a drone's local feature vector to an
aggregated fleet statistic. Out of scope: flight control, physical drone
security, and anything not implemented in this repository (there is no real
network stack, no real drone, no real fleet operator console).

## Threats

| # | Threat | Addressed by this prototype? | How / why not |
|---|---|---|---|
| T1 | Compromised cloud / fleet aggregator reads raw sensor data | **Yes, for data classified ENCRYPTED/AGGREGATABLE** | The aggregator only ever holds ciphertexts (`lib/ckks/ckksBackend.ts`) and the result of a homomorphic sum; it never holds the secret key (see `apps/fleet/compare.ts`). A compromised aggregator can see ciphertext sizes, timing, and the final decrypted aggregate (if it also somehow obtains the secret key — see T9), but not individual plaintext feature vectors. |
| T2 | Malicious fleet operator requests individual drone data | **Partially** | The policy engine (`lib/policy/scheduler.ts`) classifies data and routes LOCAL_ONLY data so it never leaves the device at all. It does not defend against a modified/malicious drone firmware that ignores its own policy engine — that is a supply-chain/firmware-integrity problem this prototype does not solve (see T10). |
| T3 | Intercepted network traffic between drone and aggregator | **Yes, for ENCRYPTED/AGGREGATABLE data** | An interceptor sees only CKKS ciphertext bytes, which (under the standard RLWE hardness assumption CKKS relies on) do not reveal the plaintext. Not addressed: traffic analysis (message sizes/timing can leak which policy branch fired — see Limitations). |
| T4 | Compromised peer drone in the fleet | **Not addressed** | This prototype has no drone-to-drone authentication or peer trust model. A compromised drone could submit a false "ciphertext" into the fleet-aggregation sum, corrupting the aggregate (a malleability/integrity issue CKKS alone does not prevent — it provides confidentiality, not authenticated integrity). A real deployment needs signed ciphertexts or a MAC-then-encrypt / authenticated-aggregation scheme on top. |
| T5 | Malicious/falsified telemetry from a legitimate drone | **Not addressed** | No plausibility/anomaly-bounds checking is applied to drone-reported values before encryption. A compromised-but-authenticated drone can report any feature vector it likes. |
| T6 | Replay attack (resending an old encrypted message) | **Not addressed** | No sequence numbers, timestamps-with-authentication, or freshness check is implemented in `apps/fleet/compare.ts`. A production system would need one. |
| T7 | Model/aggregate poisoning (one or a few drones skew the fleet statistic) | **Not addressed** | Homomorphic summation has no robust-statistics property; one outlier ciphertext shifts the mean exactly as much in CKKS as it would in plaintext. Not a cryptographic problem — a statistics/robustness problem out of scope here. |
| T8 | Unauthorized firmware / compromised software update | **Not addressed** | `lib/security/manifest.ts` explicitly reports `updateMechanism: "None implemented."` There is no signing or OTA mechanism in this prototype. |
| T9 | Stolen device (physical access to a drone or the key holder) | **Partially** | If the secret key lives only with the fleet operator (not on drones — drones only ever hold the *public* key, per `lib/ckks/ckksBackend.ts`'s `Encryptor`/`Decryptor` split), a stolen drone exposes only that drone's own local LOCAL_ONLY data, never other drones' data and never the ability to decrypt the fleet aggregate. A stolen key-holder device is a full compromise — standard key-management practice (HSM, key rotation) is out of scope here. |
| T10 | Side-channel attacks (timing, power) on the CKKS implementation | **Not addressed** | This prototype uses node-seal's WASM build of Microsoft SEAL as-is. No side-channel hardening (constant-time guarantees, power-analysis resistance) is claimed or verified. This is exactly the kind of property real E1 hardware validation (not yet performed — see Limitations) would need to assess. |

## What this threat model deliberately does not claim

- **Not** a certified or audited security boundary.
- **Not** protection against a fully malicious drone that ignores its own
  policy engine (firmware integrity is a separate, unsolved problem here).
- **Not** protection against traffic analysis, replay, or aggregate
  poisoning (T4–T7) — CKKS provides confidentiality of values, not
  authenticity, freshness, or robustness.
- **Not** a statement about Microsoft SEAL's own side-channel resistance
  (T10) — that is upstream's property to claim or disclaim, not this
  prototype's.

See also `lib/security/manifest.ts` for the machine-readable companion to
this document (crypto inventory, dependency list, update-policy status) and
`docs/README.md`'s CRA-readiness section for how this fits into a larger
evidence-generation story.
