// A feature map as an image: its own window maps values linearly to the 256 colours of a table, and points without a
// value (NaN: no pixel pairs in the window) stay transparent.

import type { FeatureMapInfo } from '@glcm/api';
import type { ColorTable } from '../image/colorTables';

export interface ValueRange {
  min: number;
  max: number;
}

/** Smallest and largest finite value, or null when there is none */
export function finiteRange(values: ArrayLike<number>): ValueRange | null {
  let min = Infinity;
  let max = -Infinity;
  for (let i = 0; i < values.length; i += 1) {
    const value = values[i];
    if (Number.isFinite(value)) {
      min = Math.min(min, value);
      max = Math.max(max, value);
    }
  }
  return min <= max ? { min, max } : null;
}

/** Nearest-rank percentiles of the finite values (by default 0.5 % and 99.5 %, as for the image), or null */
export function percentileWindow(values: ArrayLike<number>, low = 0.005, high = 0.995): ValueRange | null {
  let count = 0;
  for (let i = 0; i < values.length; i += 1) {
    if (Number.isFinite(values[i])) {
      count += 1;
    }
  }
  if (count === 0) {
    return null;
  }
  const sorted = new Float64Array(count);
  let next = 0;
  for (let i = 0; i < values.length; i += 1) {
    if (Number.isFinite(values[i])) {
      sorted[next] = values[i];
      next += 1;
    }
  }
  sorted.sort();
  const rank = (fraction: number) => sorted[Math.min(count, Math.max(1, Math.ceil(count * fraction))) - 1];
  return { min: rank(low), max: rank(high) };
}

/** Colour index 0-255 of a value, or -1 for a value that is not finite */
export function displayIndex(value: number, window: ValueRange): number {
  if (!Number.isFinite(value)) {
    return -1;
  }
  const span = window.max - window.min;
  if (!(span > 0)) {
    return value < window.min ? 0 : 255;
  }
  return Math.min(255, Math.max(0, Math.round(((value - window.min) / span) * 255)));
}

/** RGBA of the map, one pixel per grid point */
export function renderMapRgba(values: ArrayLike<number>, window: ValueRange, table: ColorTable): Uint8ClampedArray<ArrayBuffer> {
  const rgba = new Uint8ClampedArray(values.length * 4);
  for (let i = 0; i < values.length; i += 1) {
    const index = displayIndex(values[i], window);
    if (index >= 0) {
      const offset = i * 4;
      rgba[offset] = table.rgb[index * 3];
      rgba[offset + 1] = table.rgb[index * 3 + 1];
      rgba[offset + 2] = table.rgb[index * 3 + 2];
      rgba[offset + 3] = 255;
    }
  }
  return rgba;
}

/** The value of the grid point whose block contains an image pixel, or null outside the map */
export function valueAt(info: Pick<FeatureMapInfo, 'columns' | 'rows' | 'step'>, values: ArrayLike<number>, x: number, y: number): number | null {
  const column = Math.floor(x / info.step);
  const row = Math.floor(y / info.step);
  if (column < 0 || row < 0 || column >= info.columns || row >= info.rows) {
    return null;
  }
  return values[row * info.columns + column];
}

/** Four significant digits, or exponential notation for very large and very small values */
export function formatMapValue(value: number): string {
  if (Number.isNaN(value)) {
    return '–';
  }
  const magnitude = Math.abs(value);
  if (!Number.isFinite(value) || (magnitude !== 0 && (magnitude >= 1e5 || magnitude < 1e-3))) {
    return value.toExponential(3);
  }
  return String(Number(value.toPrecision(4)));
}
