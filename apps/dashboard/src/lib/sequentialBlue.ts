// Linear RGB interpolation across the dataviz skill's sequential blue ramp
// endpoints (references/palette.md step100 -> step700). An approximation of
// the documented OKLab-correct ramp, acceptable for this one utility map —
// the validated ramp itself is used verbatim in BottleneckChart/BytesChart.
const LIGHT = [0xcd, 0xe2, 0xfb];
const DARK = [0x0d, 0x36, 0x6b];

export function sequentialBlue(t: number): string {
  const clamped = Math.max(0, Math.min(1, t));
  const rgb = LIGHT.map((c, i) => Math.round(c + (DARK[i] - c) * clamped));
  return `rgb(${rgb.join(',')})`;
}
