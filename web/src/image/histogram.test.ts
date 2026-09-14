import { describe, expect, it } from 'vitest';
import { histogramPath, valueToHistogramX } from './histogram';

describe('histogramPath', () => {
  it('draws linear steps scaled to the peak', () => {
    expect(histogramPath([0, 2, 4, 1], 8, 10, 'linear')).toBe('M0 10V10H2V5H4V0H6V7.5H8V10Z');
  });

  it('uses log(1 + count) by default', () => {
    const path = histogramPath([0, 1000000, 1], 3, 100);
    expect(path.startsWith('M0 100V100H1V0H2V')).toBe(true);
    const lowBar = Number(/H2V([\d.]+)H3/.exec(path)?.[1]);
    expect(lowBar).toBeCloseTo(100 - (Math.log(2) / Math.log(1000001)) * 100, 1);
  });

  it('draws a flat line for an empty histogram', () => {
    expect(histogramPath(new Array<number>(256).fill(0), 256, 40)).toBe('M0 40H256');
  });
});

describe('valueToHistogramX', () => {
  it('maps values to bins', () => {
    expect(valueToHistogramX(0, 255, 256)).toBe(0);
    expect(valueToHistogramX(128, 255, 256)).toBe(128);
    expect(valueToHistogramX(32768, 65535, 256)).toBe(128);
  });
});
