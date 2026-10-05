import { modMulScalar } from './modular.js';

/**
 * Reference iterative radix-2 Cooley-Tukey NTT / inverse-NTT, implemented
 * from scratch for kernel-level benchmarking (lib/kernels, tools/bench/kernel-bench.ts).
 *
 * This is a HOST REFERENCE kernel, not SEAL's internal NTT — node-seal
 * (Microsoft SEAL compiled to WASM) does not expose its NTT as a standalone
 * callable, so there is no way to time "SEAL's NTT" in isolation. This
 * kernel exists to produce a real, measured butterfly-network workload with
 * the same asymptotic shape (O(N log N) modular multiply-adds) so the
 * bottleneck report and E1 dataflow-mapping discussion have genuine timing
 * data to reason about, not an invented percentage.
 *
 * NTT_PRIME = 998244353 = 119 * 2^23 + 1 is a standard NTT-friendly prime
 * (ubiquitous in competitive programming) with primitive root 3; it supports
 * polynomial lengths up to 2^23, covering every N in lib/ckks/paramSets.ts.
 */
export const NTT_PRIME = 998244353n;
const PRIMITIVE_ROOT = 3n;

function modPow(base: bigint, exp: bigint, mod: bigint): bigint {
  base %= mod;
  let result = 1n;
  while (exp > 0n) {
    if (exp & 1n) result = (result * base) % mod;
    base = (base * base) % mod;
    exp >>= 1n;
  }
  return result;
}

function modInverse(a: bigint, mod: bigint): bigint {
  return modPow(a, mod - 2n, mod);
}

/** nth root of unity mod NTT_PRIME, for n a power of two dividing NTT_PRIME-1. */
function nthRootOfUnity(n: number): bigint {
  const order = NTT_PRIME - 1n;
  if (order % BigInt(n) !== 0n) {
    throw new Error(`NTT_PRIME-1 is not divisible by n=${n}; cannot build an n-th root of unity`);
  }
  return modPow(PRIMITIVE_ROOT, order / BigInt(n), NTT_PRIME);
}

function bitReversePermute<T>(a: T[]): T[] {
  const n = a.length;
  const bits = Math.log2(n);
  if (!Number.isInteger(bits)) throw new Error('NTT length must be a power of two');
  const out = new Array<T>(n);
  for (let i = 0; i < n; i++) {
    let rev = 0;
    for (let b = 0; b < bits; b++) {
      if (i & (1 << b)) rev |= 1 << (bits - 1 - b);
    }
    out[rev] = a[i];
  }
  return out;
}

export interface NttStats {
  readonly butterflyOps: number;
  readonly stages: number;
}

/**
 * In-place-style iterative NTT. Returns the transformed coefficients and the
 * butterfly-operation / stage counts actually executed, for arithmetic-
 * intensity reporting (section 4/5: "operation count").
 */
export function ntt(coeffs: readonly bigint[], inverse = false): { result: bigint[]; stats: NttStats } {
  const n = coeffs.length;
  let a = bitReversePermute(coeffs.slice());
  const rootN = nthRootOfUnity(n);
  const root = inverse ? modInverse(rootN, NTT_PRIME) : rootN;

  let butterflyOps = 0;
  let stages = 0;
  for (let len = 2; len <= n; len <<= 1) {
    stages++;
    const wLen = modPow(root, BigInt(n / len), NTT_PRIME);
    for (let i = 0; i < n; i += len) {
      let w = 1n;
      for (let j = 0; j < len / 2; j++) {
        const u = a[i + j];
        const v = modMulScalar(a[i + j + len / 2], w, NTT_PRIME);
        a[i + j] = (u + v) % NTT_PRIME;
        a[i + j + len / 2] = (u - v + NTT_PRIME) % NTT_PRIME;
        w = modMulScalar(w, wLen, NTT_PRIME);
        butterflyOps++;
      }
    }
  }

  if (inverse) {
    const nInv = modInverse(BigInt(n), NTT_PRIME);
    a = a.map((x) => modMulScalar(x, nInv, NTT_PRIME));
  }

  return { result: a, stats: { butterflyOps, stages } };
}

/**
 * Cyclic polynomial multiplication mod (X^n - 1) via NTT, for an end-to-end
 * kernel sanity check. Note: RLWE schemes like CKKS use the *negacyclic*
 * ring mod (X^n + 1), which needs a 2n-th root of unity and a twist — this
 * kernel does not implement that twist, so it is a representative NTT
 * workload (same O(N log N) butterfly structure) and not a drop-in
 * replacement for SEAL's polynomial multiplication.
 */
export function nttMultiply(a: readonly bigint[], b: readonly bigint[]): bigint[] {
  const n = a.length;
  if (b.length !== n) throw new Error('operand length mismatch');
  const { result: fa } = ntt(a, false);
  const { result: fb } = ntt(b, false);
  const fc = fa.map((v, i) => modMulScalar(v, fb[i], NTT_PRIME));
  const { result: c } = ntt(fc, true);
  return c;
}
