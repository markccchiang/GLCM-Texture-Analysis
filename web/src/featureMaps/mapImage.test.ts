import { describe, expect, it } from 'vitest';
import { colorTableById } from '../image/colorTables';
import { displayIndex, finiteRange, formatMapValue, percentileWindow, renderMapRgba, valueAt } from './mapImage';

describe('feature map image', () => {
  it('finds the range and percentiles of the finite values', () => {
    const values = new Float32Array([Number.NaN, 3, -1, Number.POSITIVE_INFINITY, 7]);
    expect(finiteRange(values)).toEqual({ min: -1, max: 7 });
    expect(finiteRange(new Float32Array([Number.NaN]))).toBeNull();
    const ramp = Float32Array.from({ length: 1000 }, (_, i) => 999 - i);
    // Nearest rank: ceil(1000 × 0.005) = 5th and ceil(1000 × 0.995) = 995th smallest
    expect(percentileWindow(ramp)).toEqual({ min: 4, max: 994 });
    expect(percentileWindow([Number.NaN, 2])).toEqual({ min: 2, max: 2 });
    expect(percentileWindow([])).toBeNull();
  });

  it('maps the window linearly to the colour indices', () => {
    const window = { min: 10, max: 20 };
    expect(displayIndex(10, window)).toBe(0);
    expect(displayIndex(15, window)).toBe(128);
    expect(displayIndex(20, window)).toBe(255);
    expect(displayIndex(-5, window)).toBe(0);
    expect(displayIndex(99, window)).toBe(255);
    expect(displayIndex(Number.NaN, window)).toBe(-1);
    expect(displayIndex(4, { min: 5, max: 5 })).toBe(0);
    expect(displayIndex(5, { min: 5, max: 5 })).toBe(255);
  });

  it('colours the map and leaves points without a value transparent', () => {
    const viridis = colorTableById('viridis');
    const rgba = renderMapRgba(new Float32Array([0, Number.NaN, 1]), { min: 0, max: 1 }, viridis);
    expect(Array.from(rgba)).toEqual([68, 1, 84, 255, 0, 0, 0, 0, 253, 231, 37, 255]);
  });

  it('reads the value of the block under an image pixel', () => {
    const info = { columns: 3, rows: 2, step: 4 };
    const values = [0, 1, 2, 3, 4, 5];
    expect(valueAt(info, values, 0, 0)).toBe(0);
    expect(valueAt(info, values, 11, 7)).toBe(5);
    expect(valueAt(info, values, 12, 0)).toBeNull();
    expect(valueAt(info, values, -1, 0)).toBeNull();
  });

  it('formats values with four significant digits', () => {
    expect(formatMapValue(12.34567)).toBe('12.35');
    expect(formatMapValue(0)).toBe('0');
    expect(formatMapValue(123456)).toBe('1.235e+5');
    expect(formatMapValue(0.0001234)).toBe('1.234e-4');
    expect(formatMapValue(Number.NaN)).toBe('–');
  });
});
