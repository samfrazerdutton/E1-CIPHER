/**
 * CKKS parameter sets benchmarked by tools/bench/ckks-bench.ts.
 *
 * `coeffModulusBits` follows the standard CKKS pattern of large "special"
 * primes at the ends of the chain (for the first level and for key
 * switching) with scale-matched primes in between (one consumed per
 * rescale). Multiplicative depth is `coeffModulusBits.length - 1`.
 *
 * `maxBitsAt128` values come from HomomorphicEncryption.org's standard
 * security tables (queried live via SEAL's CoeffModulus.MaxBitCount — see
 * scripts/_smoke.mjs history in the commit log for the raw query). A
 * parameter set whose chain exceeds this budget cannot reach 128-bit
 * security and is marked unsupported rather than silently clamped.
 */
export interface CkksParamSet {
  readonly id: string;
  readonly logN: number;
  readonly polyModulusDegree: number;
  readonly coeffModulusBits: readonly number[];
  readonly scaleBits: number;
  readonly notes: string;
}

export const CKKS_PARAM_SETS: readonly CkksParamSet[] = [
  {
    id: 'N1024',
    logN: 10,
    polyModulusDegree: 1024,
    coeffModulusBits: [27],
    scaleBits: 20,
    notes:
      'Single-modulus chain (128-bit security budget at N=1024 is only 27 bits). ' +
      'Supports encode/encrypt/add/decrypt/decode only — no headroom for a second ' +
      'modulus, so multiply+rescale and rotation are not attempted at this parameter set.',
  },
  {
    id: 'N2048',
    logN: 11,
    polyModulusDegree: 2048,
    coeffModulusBits: [18, 17, 18],
    scaleBits: 16,
    notes:
      'Three-prime chain, multiplicative depth 1. A naive 2-prime chain ([23,23], sum 46 <= the ' +
      '54-bit budget) was tried first but SEAL rejects it at multiply time ("scale out of bounds") ' +
      'and at rescale time ("end of modulus switching chain reached") — with hybrid key-switching ' +
      'enabled, 2 primes give zero usable multiplicative levels in practice, not 1. Confirmed ' +
      'empirically (see commit history / docs/bottleneck-report.md); 3 smaller primes are required ' +
      'to get one real multiply+rescale at this N, at the cost of a larger relative error (~4e-2).',
  },
  {
    id: 'N4096',
    logN: 12,
    polyModulusDegree: 4096,
    coeffModulusBits: [33, 27, 33],
    scaleBits: 24,
    notes:
      'Three-prime chain. Verified to support one multiply+relinearize+rescale+rotate at this ' +
      'scale (see N2048 note on why prime count does not map 1:1 to usable depth under hybrid ' +
      'key-switching). Full achievable chain depth beyond one level is not exercised by this ' +
      'benchmark and is not claimed.',
  },
  {
    id: 'N8192',
    logN: 13,
    polyModulusDegree: 8192,
    coeffModulusBits: [60, 40, 40, 60],
    scaleBits: 36,
    notes:
      'Classic SEAL example chain. Verified to support one multiply+relinearize+rescale+rotate ' +
      'at this scale; deeper chaining not exercised by this benchmark and not claimed.',
  },
  {
    id: 'N16384',
    logN: 14,
    polyModulusDegree: 16384,
    coeffModulusBits: [60, 50, 50, 50, 50, 50, 60],
    scaleBits: 46,
    notes:
      'Deep chain. Verified to support one multiply+relinearize+rescale+rotate at this scale; ' +
      'deeper chaining not exercised by this benchmark and not claimed.',
  },
] as const;

export function totalCoeffModulusBits(p: CkksParamSet): number {
  return p.coeffModulusBits.reduce((a, b) => a + b, 0);
}
