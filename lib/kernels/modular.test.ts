import { test } from 'node:test';
import assert from 'node:assert/strict';
import { modMulScalar, MontgomeryContext } from './modular.js';
import { NTT_PRIME } from './ntt.js';

test('Montgomery multiplication matches naive modular multiplication', () => {
  const mont = new MontgomeryContext(NTT_PRIME, 64n);
  const x = 123456789n;
  const y = 987654321n;
  const expected = modMulScalar(x, y, NTT_PRIME);
  const got = mont.fromMontgomery(mont.mulMontgomery(mont.toMontgomery(x), mont.toMontgomery(y)));
  assert.equal(got, expected);
});
