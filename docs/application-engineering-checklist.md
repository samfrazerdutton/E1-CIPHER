# Application Engineering Checklist

What's actually true of this repository right now, section by section —
written as a checklist an applications engineer would use to evaluate
whether a codebase is ready to hand to a hardware team, not as a sales
sheet. Unchecked items say why, not just that they're unchecked.

## Build

- [x] CMake 3.20+, builds with Ninja + Clang 22 (no Visual Studio
      required — verified on a machine with neither installed; see
      `docs/architecture-assessment.md`).
- [x] Dependencies (Microsoft SEAL, doctest) fetched via `FetchContent`,
      pinned to exact tags (`v4.1.2`, `v2.4.11`) — a fresh clone builds
      without a manual dependency-install step.
- [x] `-DTARGET_E1=ON` fails configuration with an explicit message
      (`cmake/FindE1Toolchain.cmake`) rather than silently falling back to
      a host build under an E1 label.
- [ ] A pinned/vendored SEAL (rather than fetched from `microsoft/SEAL`'s
      `v4.1.2` tag at configure time) for fully offline/air-gapped builds —
      not done; this repo assumes network access at configure time, same
      as the TS-era prototype's `npm install` did.

## Runtime

- [x] One CLI binary (`e1cipher`) with subcommands (`inspect`, `fleet`,
      `platform`, `security`) rather than a pile of one-off executables
      sharing no entry point.
- [x] `apps/drone_inspection`, `apps/fleet_gateway` both run the real
      pipeline end to end (not a scripted/animated narrative).
- [ ] A long-running/daemon mode, config hot-reload, graceful shutdown —
      not built. This is a CLI reference application, not a service.

## Performance

- [x] Real `std::chrono::steady_clock` measurements for every CKKS op and
      both reference kernels, with mean/median/stddev/min/max over 20-30
      trials and a warm-up phase (`benchmarks/crypto`, `benchmarks/kernels`).
- [x] Native CKKS (via real SEAL) is measurably faster than the TS-era
      WASM build at every parameter set (e.g. ~2.3x on N16384 rescale) —
      reported because it's a real, checkable difference between the two
      implementations, not asserted.
- [ ] CPU performance counters (cache misses, branch mispredicts) — not
      implemented. `std::chrono` wall-clock only; no `perf`/ETW
      integration. Listed honestly as not done rather than estimated.
- [ ] Thread-scaling benchmark (1/2/4/8 threads) — not implemented this
      pass. The embarrassingly-parallel parts of this workload (per-drone
      encrypt, independent CKKS ops) are identified in
      `docs/e1-application-mapping.md` but not yet measured under
      `std::thread`/OpenMP scaling.
- [ ] SIMD/vector-path benchmarking — not implemented. See
      `docs/e1-application-mapping.md`'s open questions instead of a
      fabricated vectorization story.

## Memory

- [x] `TelemetrySample`/`FeatureVector` are documented as trivially
      copyable, fixed-size, no owned heap allocation (`static_assert`s in
      their headers enforce this, not just a comment).
- [x] `CkksBackend` uses the pimpl idiom specifically so SEAL's heavy
      headers don't leak into every caller's include graph — a real
      compile-time/API-surface decision, not decoration.
- [x] Every `CryptoBackend` implementation uses RAII (`std::unique_ptr`
      for `CkksBackend::Impl`); no manual `new`/`delete` pairing in
      application code.
- [x] AddressSanitizer + UndefinedBehaviorSanitizer build
      (`-DE1CIPHER_SANITIZER=address-undefined`) passes clean on all four
      test binaries and all CLI subcommands (verified, not assumed — see
      `docs/architecture-assessment.md`'s toolchain section for the one
      build wrinkle this surfaced and fixed: Debug-CRT linkage, routed
      around via RelWithDebInfo).

## Security

- [x] Fail-closed policy behavior is a tested invariant, not an
      aspiration: `tests/security/test_fail_closed.cpp` and
      `src/diagnostics/security_gate.cpp`'s runtime checks both verify
      unknown classification, missing crypto backend, and LOCAL_ONLY data
      all resolve to `Blocked`/no-transmit — never a permissive default.
- [x] Key custody is structural, not policy: `apps/fleet_gateway/fleet.cpp`
      models the gateway role never constructing a `Decryptor`/`SecretKey`
      at all, not merely "not calling decrypt."
- [ ] Authenticated/signed ciphertexts (replay/tamper protection) — not
      implemented; see `docs/threat-model.md` T4/T6, explicitly listed as
      not mitigated.

## Deployment

- [ ] Packaging (installer, container image, systemd unit) — not built.
      This is source handed to a hardware team, not a shipped product.
- [ ] A signed release/update mechanism — not built; see
      `docs/cra-evidence/05-security-update-policy.md`, which says so
      plainly rather than describing a mechanism that doesn't exist.

## Diagnostics

- [x] `e1cipher platform` reports real host info (CPU brand string via
      `__cpuid`, compiler, C++ standard, git commit, E1-toolchain-on-PATH
      check) — useful for debugging *where* a build actually ran.
- [x] `e1cipher security` is a real runtime gate (constructs an actual
      `CkksBackend`, runs actual policy decisions) producing
      PASS/WARN/FAIL with reasons, not a static report.
- [ ] Structured logging / telemetry export for a running fleet — the
      `AuditEvent` type and `to_json_line` exist
      (`include/e1cipher/diagnostics/audit.hpp`) but nothing in the
      current apps actually wires them up to emit a persisted audit log
      yet.

## Testing

- [x] CTest, 4 binaries, doctest framework, covering kernel correctness
      (NTT round-trip, convolution-vs-naive, Montgomery-vs-naive),
      policy determinism, CKKS/plaintext/mock backend round-trips, and
      fail-closed security behavior specifically.
- [x] All tests pass both in a plain Release build and under
      ASan+UBSan.
- [ ] Fuzz testing (e.g. libFuzzer against the policy scheduler's inputs
      or the CKKS serialization round-trip) — not implemented.
- [ ] Integration tests that actually exercise the CLI binary end-to-end
      (spawn the process, check stdout) — current tests call library
      functions directly; nothing shells out to `e1cipher.exe` and asserts
      on its output.

## Hardware portability

- [x] `platform/host`, `platform/e1`, `platform/mock` directories exist
      with a real (`HostPlatform`) and an explicitly-stubbed (`E1Platform`,
      always `ResultClass::NotMeasured`) implementation.
- [x] `cmake/FindE1Toolchain.cmake` + `-DTARGET_E1=ON` define the exact
      integration point a real E1 backend would need, without guessing at
      undocumented E1 specifics.
- [ ] Actual E1 hardware validation — blocked entirely on hardware/
      toolchain access this environment does not have. Not estimated,
      not simulated with invented numbers.

## Toolchain integration

- [x] `e1cipher platform` detects `effcc` on `PATH` at runtime
      independent of what was detected at CMake configure time (two
      separate, both-real checks — see
      `src/diagnostics/platform_report.cpp`).
- [ ] An actual effcc compile of `src/crypto/kernels/` — blocked on
      toolchain access.

## Customer validation

- [ ] Nothing here has been run past an actual Efficient Computer
      engineer or customer workload. This checklist is a self-assessment,
      not a validation result.

## Failure handling

- [x] `CkksBackend::create()` returns `nullptr` on an invalid parameter
      set rather than throwing from deep inside SEAL; every caller checks
      it (`inspect.cpp`, `fleet.cpp`, `security_gate.cpp`).
- [x] The policy scheduler's fail-closed behavior (see Security above) is
      itself a failure-handling story, not just a security one: the
      system has a well-defined, tested answer for "what happens when an
      input is unrecognized or a dependency is unavailable," which is
      exactly the question that matters in the field.
