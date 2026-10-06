# 60-Second Demo

```sh
git clone https://github.com/samfrazerdutton/E1-CIPHER.git && cd E1-CIPHER
cmake -G Ninja -B build .   # add -DCMAKE_CXX_COMPILER=clang++ if needed
cmake --build build -j
./build/apps/e1cipher runtime-demo
```

That one command tells the whole story:

1. **Workload** — a realistic industrial-inspection operation set (camera
   frame, thermal/vibration features, GPS, battery diagnostic).
2. **Policy** — each operation's real sensitivity classification, printed
   live.
3. **Placement** — where the runtime actually routed each operation
   (`LOCAL`/`ENCRYPTED`/`ACCELERATED`/`REMOTE`), and why, in one sentence.
4. **Measured cost** — real Microsoft SEAL CKKS numbers: encrypt+add
   latency, ciphertext size, the 2,733x ciphertext-expansion finding —
   measured on whatever machine you just ran this on, not hardcoded.
5. **Encrypted operation** — `vibration_feature` and
   `fleet_anomaly_statistic` actually route through CKKS.
6. **Accelerator candidate** — those same two operations are flagged
   `ACCELERATED`, backed by a real NTT kernel run
   (`runtime::HostExecutionBackend`), not a label with nothing behind it.
7. **Final architecture decision** — selective encrypted execution,
   enabled, with the real bandwidth numbers for "what if we'd encrypted
   everything" vs. "what we actually did."

For the fuller story (three experiments, the full application story, the
hardware hypothesis):

```sh
./build/apps/e1cipher experiment selective     # the core thesis: plaintext vs encrypted vs selective
./build/apps/e1cipher experiment accelerator   # which CKKS sub-op is the real bottleneck, and which isn't despite measuring higher
./build/apps/e1cipher experiment scaling       # 1/10/100/1000 devices
./build/apps/e1cipher security                 # fail-closed policy tests + crypto health, PASS/WARN/FAIL
ctest --test-dir build --output-on-failure     # 6 suites, including fail-closed regression tests
```

Total run time for all of the above: well under two minutes on ordinary
hardware.
