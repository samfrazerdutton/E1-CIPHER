import type { Ciphertext } from 'node-seal';
import type { CryptoBackend } from './backend.js';
import { buildCkksContext, type CkksContext } from './context.js';
import type { CkksParamSet } from './paramSets.js';

/**
 * Real CKKS backend, backed by Microsoft SEAL via node-seal (WASM). Multiply
 * eagerly relinearizes and rescales so the result is a normalized ciphertext
 * ready for another operation — matching how an application (not a
 * micro-benchmark) would actually chain CKKS ops. tools/bench/ckks-bench.ts
 * measures the unbundled sub-steps separately for the bottleneck report.
 */
export async function createCkksBackend(paramSet: CkksParamSet): Promise<CryptoBackend<Ciphertext> & { ctx: CkksContext }> {
  const ctx = await buildCkksContext(paramSet);
  if (!ctx.parametersSet) {
    throw new Error(
      `CKKS parameter set ${paramSet.id} failed SEALContext.parametersSet() ` +
        `(requested ${paramSet.coeffModulusBits.join('+')} bits vs ${ctx.maxBitsAt128}-bit budget at 128-bit security)`,
    );
  }

  if (!ctx.relinKeys) {
    throw new Error(
      `CKKS parameter set ${paramSet.id} has no room for key-switching (single-modulus chain); ` +
        'multiply/relinearize is unavailable, so this backend cannot be constructed for it.',
    );
  }

  const scale = Math.pow(2, paramSet.scaleBits);
  const { seal, encoder, encryptor, decryptor, evaluator, relinKeys } = ctx;

  return {
    name: `CKKS(${paramSet.id})`,
    kind: 'ckks',
    isRealCrypto: true,
    slotCount: ctx.slotCount,
    ctx,
    encrypt(values) {
      const plain = new seal.Plaintext();
      encoder.encode(values, scale, plain);
      const cipher = new seal.Ciphertext();
      encryptor.encrypt(plain, cipher);
      return cipher;
    },
    decrypt(cipher) {
      const plain = new seal.Plaintext();
      decryptor.decrypt(cipher, plain);
      return encoder.decodeFloat64(plain) as Float64Array;
    },
    add(a, b) {
      const out = new seal.Ciphertext();
      evaluator.add(a, b, out);
      return out;
    },
    multiply(a, b) {
      const out = new seal.Ciphertext();
      evaluator.multiply(a, b, out);
      evaluator.relinearizeInplace(out, relinKeys);
      evaluator.rescaleToNextInplace(out);
      return out;
    },
    byteLength(cipher) {
      const arr = cipher.saveToArray(seal.ComprModeType.none) as Uint8Array | { length: number };
      return 'byteLength' in arr ? (arr as Uint8Array).byteLength : (arr as { length: number }).length;
    },
    dispose() {
      // node-seal objects are WASM-backed; let them be GC'd via FinalizationRegistry.
      // Explicit .delete() calls are omitted here because this backend's objects
      // (keys, encoder, context) are reused across many benchmark iterations.
    },
  };
}
