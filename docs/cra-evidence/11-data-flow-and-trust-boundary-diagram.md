# Data Flow & Trust Boundary Diagram

```mermaid
flowchart TB
  subgraph DRONE[Drone -- holds PUBLIC key only]
    SENSOR[Raw sensors: camera/LiDAR/IMU] -->|LOCAL_ONLY, never leaves| FUSION[Sensor fusion]
    FUSION --> FEATURE[Feature vector, 6 scalars]
    FEATURE --> POLICY[Policy engine: classify + place]
    POLICY -->|ENCRYPTED/AGGREGATABLE| ENCRYPT[CKKS encrypt]
    POLICY -->|PLAINTEXT, low sensitivity| PLAINSEND[Plaintext send]
  end
  subgraph BOUNDARY1[" "]
  end
  subgraph AGGREGATOR[Aggregator -- UNTRUSTED, holds NO key]
    ENCRYPT -->|ciphertext only| SUM[Homomorphic sum across fleet]
    PLAINSEND -->|plaintext, e.g. battery| DASH[Fleet health dashboard]
  end
  subgraph BOUNDARY2[" "]
  end
  subgraph OPERATOR[Fleet operator -- holds SECRET key]
    SUM -->|aggregate ciphertext| DECRYPT[Decrypt]
    DECRYPT --> MEAN[Fleet-wide mean/statistic only]
  end
```

Trust boundaries (the horizontal `BOUNDARY` rows above): crossing **drone -> aggregator** exposes only
ciphertext bytes and explicitly-plaintext-classified fields (see `lib/policy/classification.ts`);
crossing **aggregator -> operator** is where decryption actually happens, and only on the aggregate, never
on an individual drone's ciphertext. Compare against `docs/threat-model.md` T1-T3 and
`apps/redteam/intercept-demo.ts` for a concrete demonstration of what crosses each boundary.
