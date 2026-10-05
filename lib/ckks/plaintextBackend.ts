import type { CryptoBackend } from './backend.js';

/**
 * Identity backend: no encoding, no encryption, no confidentiality. Used as
 * the "what would this cost with no cryptography at all" baseline in the
 * fleet-aggregation comparison (apps/fleet/compare.ts) and policy engine.
 */
export function createPlaintextBackend(slotCount: number): CryptoBackend<Float64Array> {
  return {
    name: 'Plaintext',
    kind: 'plaintext',
    isRealCrypto: false,
    slotCount,
    encrypt(values) {
      return values.slice();
    },
    decrypt(cipher) {
      return cipher;
    },
    add(a, b) {
      const out = new Float64Array(a.length);
      for (let i = 0; i < a.length; i++) out[i] = a[i] + b[i];
      return out;
    },
    multiply(a, b) {
      const out = new Float64Array(a.length);
      for (let i = 0; i < a.length; i++) out[i] = a[i] * b[i];
      return out;
    },
    byteLength(cipher) {
      return cipher.byteLength;
    },
    dispose() {},
  };
}
