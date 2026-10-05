import { test } from 'node:test';
import assert from 'node:assert/strict';
import { ntt, nttMultiply, NTT_PRIME } from './ntt.js';
import { modMulScalar } from './modular.js';

function naiveCyclicConv(a: bigint[], b: bigint[], q: bigint): bigint[] {
  const n = a.length;
  const out = new Array<bigint>(n).fill(0n);
  for (let i = 0; i < n; i++) {
    for (let j = 0; j < n; j++) {
      const k = (i + j) % n;
      out[k] = (out[k] + modMulScalar(a[i], b[j], q)) % q;
    }
  }
  return out;
}

test('NTT forward then inverse round-trips', () => {
  const n = 16;
  const a = Array.from({ length: n }, (_, i) => BigInt(i + 1));
  const { result: fwd } = ntt(a, false);
  const { result: back } = ntt(fwd, true);
  assert.deepEqual(back, a);
});

test('nttMultiply matches naive O(n^2) cyclic convolution', () => {
  const n = 16;
  const a = Array.from({ length: n }, (_, i) => BigInt(i + 1));
  const b = Array.from({ length: n }, (_, i) => BigInt(2 * i + 1));
  assert.deepEqual(nttMultiply(a, b), naiveCyclicConv(a, b, NTT_PRIME));
});
