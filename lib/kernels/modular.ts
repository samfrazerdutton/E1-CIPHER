/**
 * Reference modular-arithmetic kernels, implemented from scratch in TS
 * (not calling into SEAL) so their cost can be measured and decomposed
 * independently of the WASM library — see tools/bench/kernel-bench.ts and
 * docs/bottleneck-report.md. These are HOST REFERENCE measurements: a
 * scalar BigInt baseline, useful for reasoning about where a spatial/
 * dataflow architecture could help, but not a reimplementation of SEAL's
 * internal (Shoup/Barrett-optimized, SIMD) arithmetic.
 */

/** Scalar modular multiplication via BigInt (the naive baseline). */
export function modMulScalar(a: bigint, b: bigint, q: bigint): bigint {
  return (a * b) % q;
}

/** Scalar modular addition. */
export function modAddScalar(a: bigint, b: bigint, q: bigint): bigint {
  const s = a + b;
  return s >= q ? s - q : s;
}

/**
 * Montgomery multiplication: a redundant-representation technique that
 * replaces the modulus-dependent division in modMulScalar with shifts and a
 * precomputed constant. Implemented at BigInt-word granularity (not
 * machine-word limbs) — this measures the *algorithmic* saving over naive
 * modmul on this host, not a production-grade multi-precision Montgomery
 * ladder.
 */
export class MontgomeryContext {
  readonly q: bigint;
  readonly r: bigint; // R = 2^rBits, R > q
  readonly rBits: bigint;
  readonly qInv: bigint; // -q^-1 mod R

  constructor(q: bigint, rBits = 64n) {
    this.q = q;
    this.rBits = rBits;
    this.r = 1n << rBits;
    this.qInv = MontgomeryContext.modInverse(-q, this.r);
  }

  private static modInverse(a: bigint, m: bigint): bigint {
    a = ((a % m) + m) % m;
    let [old_r, r] = [a, m];
    let [old_s, s] = [1n, 0n];
    while (r !== 0n) {
      const quotient = old_r / r;
      [old_r, r] = [r, old_r - quotient * r];
      [old_s, s] = [s, old_s - quotient * s];
    }
    return ((old_s % m) + m) % m;
  }

  toMontgomery(a: bigint): bigint {
    return (a << this.rBits) % this.q;
  }

  fromMontgomery(aBar: bigint): bigint {
    return this.redc(aBar);
  }

  private redc(t: bigint): bigint {
    const mask = this.r - 1n;
    const m = ((t & mask) * this.qInv) & mask;
    let result = (t + m * this.q) >> this.rBits;
    if (result >= this.q) result -= this.q;
    return result;
  }

  /** Multiply two Montgomery-form operands, result stays in Montgomery form. */
  mulMontgomery(aBar: bigint, bBar: bigint): bigint {
    return this.redc(aBar * bBar);
  }
}

export function modMulVectorScalar(a: readonly bigint[], b: readonly bigint[], q: bigint): bigint[] {
  const out = new Array<bigint>(a.length);
  for (let i = 0; i < a.length; i++) out[i] = modMulScalar(a[i], b[i], q);
  return out;
}

export function modMulVectorMontgomery(a: readonly bigint[], b: readonly bigint[], ctx: MontgomeryContext): bigint[] {
  const out = new Array<bigint>(a.length);
  const aBar = a.map((x) => ctx.toMontgomery(x));
  const bBar = b.map((x) => ctx.toMontgomery(x));
  for (let i = 0; i < a.length; i++) out[i] = ctx.fromMontgomery(ctx.mulMontgomery(aBar[i], bBar[i]));
  return out;
}
