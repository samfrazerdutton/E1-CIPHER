# Cryptographic Inventory & Configuration Record

_Scheme family: CKKS (approximate-arithmetic leveled homomorphic encryption, RLWE-based). Library: node-seal (Microsoft SEAL 4.1.2)._

## Parameter sets in use (`lib/ckks/paramSets.ts`)

| ID | N (poly modulus degree) | Coeff modulus chain (bits) | Total bits | Scale (bits) |
|---|---|---|---|---|
| N1024 | 1024 | [27] | 27 | 20 |
| N2048 | 2048 | [18, 17, 18] | 53 | 16 |
| N4096 | 4096 | [33, 27, 33] | 93 | 24 |
| N8192 | 8192 | [60, 40, 40, 60] | 200 | 36 |
| N16384 | 16384 | [60, 50, 50, 50, 50, 50, 60] | 370 | 46 |

## Other registered crypto backends (`lib/ckks/backend.ts`)

- Plaintext (no cryptography, baseline)
- MockEncrypted (non-cryptographic placeholder, development only)

## Key custody model

- Drones hold only the CKKS **public** key (`Encryptor`).
- The fleet operator alone holds the **secret** key (`Decryptor`).
- The aggregator role holds neither key — only `Ciphertext` objects (see `apps/fleet/compare.ts`, `apps/demo/killer-demo.ts`).
- No key rotation, HSM integration, or key-escrow mechanism is implemented in this prototype.
