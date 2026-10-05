import type { CryptoBackend } from './backend.js';

export interface MockCipher {
  readonly values: Float64Array;
  readonly inflatedByteLength: number;
}

/**
 * NOT CRYPTOGRAPHY. This backend performs plaintext arithmetic and reports a
 * configurable inflated byte length so the CryptoBackend abstraction has a
 * third, structurally "ciphertext-shaped" implementation to exercise during
 * development (e.g. wiring the policy engine) without paying real CKKS cost.
 *
 * It must never be used to produce or stand in for a reported latency,
 * throughput, or security number — those must come from createCkksBackend
 * (real) or createPlaintextBackend (real, zero-crypto baseline). Per the
 * project's anti-fabrication rule, no artificial delay is injected here:
 * timings taken against this backend would simply measure plain arithmetic
 * plus a byte-count multiplication, and reporting that as "encrypted
 * latency" would be inventing a benchmark.
 */
export function createMockEncryptedBackend(slotCount: number, expansionFactor = 40): CryptoBackend<MockCipher> {
  const bytesPerSlot = 8 * expansionFactor;
  return {
    name: `MockEncrypted(x${expansionFactor})`,
    kind: 'mock-encrypted',
    isRealCrypto: false,
    slotCount,
    encrypt(values) {
      return { values: values.slice(), inflatedByteLength: values.length * bytesPerSlot };
    },
    decrypt(cipher) {
      return cipher.values;
    },
    add(a, b) {
      const out = new Float64Array(a.values.length);
      for (let i = 0; i < out.length; i++) out[i] = a.values[i] + b.values[i];
      return { values: out, inflatedByteLength: a.inflatedByteLength };
    },
    multiply(a, b) {
      const out = new Float64Array(a.values.length);
      for (let i = 0; i < out.length; i++) out[i] = a.values[i] * b.values[i];
      return { values: out, inflatedByteLength: a.inflatedByteLength };
    },
    byteLength(cipher) {
      return cipher.inflatedByteLength;
    },
    dispose() {},
  };
}
