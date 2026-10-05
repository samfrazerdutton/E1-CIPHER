/** Plain vector add/scale over Float64Array, used as the non-modular baseline comparison point. */
export function vectorAdd(a: Float64Array, b: Float64Array): Float64Array {
  const out = new Float64Array(a.length);
  for (let i = 0; i < a.length; i++) out[i] = a[i] + b[i];
  return out;
}

export function vectorScale(a: Float64Array, s: number): Float64Array {
  const out = new Float64Array(a.length);
  for (let i = 0; i < a.length; i++) out[i] = a[i] * s;
  return out;
}
