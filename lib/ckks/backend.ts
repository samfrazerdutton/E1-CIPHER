/**
 * CryptoBackend: the abstraction that lets the rest of the system (policy
 * engine, fleet aggregation) run against CKKS, a plaintext passthrough, or a
 * mock-encrypted stand-in without being rewritten. See docs/README.md
 * section "Cryptographic agility".
 *
 * MockEncrypted is NOT cryptography — it exists only to let latency/bandwidth
 * comparisons include "ciphertext-shaped" overhead (size inflation, a fixed
 * per-op cost) when experimenting with policy logic away from the much
 * heavier real CKKS backend. It must never be presented as providing
 * confidentiality.
 */
export type BackendKind = 'ckks' | 'plaintext' | 'mock-encrypted';

export interface CryptoBackend<Cipher = unknown> {
  readonly name: string;
  readonly kind: BackendKind;
  readonly isRealCrypto: boolean;
  readonly slotCount: number;
  encrypt(values: Float64Array): Cipher;
  decrypt(cipher: Cipher): Float64Array;
  add(a: Cipher, b: Cipher): Cipher;
  multiply(a: Cipher, b: Cipher): Cipher;
  byteLength(cipher: Cipher): number;
  dispose(): void;
}
