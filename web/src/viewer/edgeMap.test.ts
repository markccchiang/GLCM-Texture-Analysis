import { describe, expect, it } from 'vitest';
import { autoEdgeLimits, roundLimit } from './edgeMap';

const STATISTICS = { sigma: 1, percentiles: { '50': 2, '90': 10, '95': 20, '99': 60 }, max: 120 };

describe('edge map limits', () => {
  it('chooses a Sobel window and Canny thresholds from the gradient percentiles', () => {
    expect(autoEdgeLimits('sobel', STATISTICS)).toEqual({ low: 0, high: 60 });
    expect(autoEdgeLimits('canny', STATISTICS)).toEqual({ low: 8, high: 20 });
  });

  it('keeps a valid Sobel window on a flat image', () => {
    const flat = { sigma: 1, percentiles: { '50': 0, '90': 0, '95': 0, '99': 0 }, max: 0 };
    const { low, high } = autoEdgeLimits('sobel', flat);
    expect(low).toBeLessThan(high);
    expect(autoEdgeLimits('canny', flat)).toEqual({ low: 0, high: 0 });
  });

  it('rounds limits to four significant digits', () => {
    expect(roundLimit(123.456)).toBe(123.5);
    expect(roundLimit(0.000123456)).toBe(0.0001235);
    expect(roundLimit(0)).toBe(0);
  });
});
