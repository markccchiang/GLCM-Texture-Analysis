import { windowLevel } from '@glcm/api';
import { describe, expect, it } from 'vitest';
import { buildLut, renderToRgba } from './lut';
import type { RawImage } from './raw';

describe('buildLut', () => {
  it('matches windowLevel for every 8-bit value', () => {
    for (const [min, max] of [
      [0, 255],
      [10, 20],
      [100, 100],
      [37, 211],
    ]) {
      const lut = buildLut(8, min, max);
      expect(lut).toHaveLength(256);
      for (let value = 0; value < 256; value += 1) {
        expect(lut[value]).toBe(windowLevel(value, min, max));
      }
    }
  });

  it('covers the 16-bit range', () => {
    const lut = buildLut(16, 1000, 3000);
    expect(lut).toHaveLength(65536);
    for (const value of [0, 999, 1000, 1001, 2000, 2999, 3000, 65535]) {
      expect(lut[value]).toBe(windowLevel(value, 1000, 3000));
    }
  });
});

describe('renderToRgba with a colour table', () => {
  it('looks the display value up in the table', () => {
    const table = new Uint8Array(768);
    for (let value = 0; value < 256; value += 1) {
      table.set([value, 255 - value, 7], value * 3);
    }
    const samples = new Uint8Array([0, 128, 255]);
    const rgba = renderToRgba({ width: 3, height: 1, bitDepth: 8, samples }, 0, 255, undefined, table);
    expect(Array.from(rgba)).toEqual([0, 255, 7, 255, 128, 127, 7, 255, 255, 0, 7, 255]);
  });
});

describe('renderToRgba', () => {
  it('renders opaque gray pixels with the window/level mapping', () => {
    const samples = new Uint16Array([0, 500, 1000, 1500, 2000, 65535]);
    const image: RawImage = { width: 3, height: 2, bitDepth: 16, samples };
    const rgba = renderToRgba(image, 500, 2000);
    expect(rgba).toHaveLength(24);
    samples.forEach((value, i) => {
      const gray = windowLevel(value, 500, 2000);
      expect(Array.from(rgba.subarray(4 * i, 4 * i + 4))).toEqual([gray, gray, gray, 255]);
    });
  });

  it('reuses an output buffer of the right size', () => {
    const image: RawImage = { width: 2, height: 1, bitDepth: 8, samples: new Uint8Array([0, 255]) };
    const output = new Uint8ClampedArray(8);
    expect(renderToRgba(image, 0, 255, output)).toBe(output);
    expect(() => renderToRgba(image, 0, 255, new Uint8ClampedArray(4))).toThrow(/expected 8/);
  });
});
