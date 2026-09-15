import { describe, expect, it } from 'vitest';
import { colorAt, colorizeRgba, colorTableById, COLOR_TABLES, cssGradient } from './colorTables';

describe('colour tables', () => {
  it('has 256 RGB colours per table', () => {
    expect(COLOR_TABLES.map((table) => table.id)).toEqual(['gray', 'inverted', 'viridis', 'magma', 'hot']);
    for (const table of COLOR_TABLES) {
      expect(table.rgb).toHaveLength(768);
    }
  });

  it('maps gray to itself and inverted to 255 minus the value', () => {
    for (const value of [0, 1, 128, 254, 255]) {
      expect(colorAt(colorTableById('gray'), value)).toEqual([value, value, value]);
      expect(colorAt(colorTableById('inverted'), value)).toEqual([255 - value, 255 - value, 255 - value]);
    }
  });

  it("keeps matplotlib's colours at the ends of the pseudo-colour tables", () => {
    expect(colorAt(colorTableById('viridis'), 0)).toEqual([68, 1, 84]);
    expect(colorAt(colorTableById('viridis'), 255)).toEqual([253, 231, 37]);
    expect(colorAt(colorTableById('magma'), 0)).toEqual([0, 0, 4]);
    expect(colorAt(colorTableById('magma'), 255)).toEqual([252, 253, 191]);
    // Hot runs from black through red and yellow to white, never getting darker
    const hot = colorTableById('hot');
    expect(colorAt(hot, 255)).toEqual([255, 255, 255]);
    for (let value = 1; value < 256; value += 1) {
      const [r, g, b] = colorAt(hot, value);
      const [pr, pg, pb] = colorAt(hot, value - 1);
      expect(r >= pr && g >= pg && b >= pb).toBe(true);
    }
  });

  it('colours gray pixels in place and leaves the gray table alone', () => {
    const pixels = new Uint8ClampedArray([0, 0, 0, 255, 255, 255, 255, 255, 100, 100, 100, 200]);
    const gray = pixels.slice();
    colorizeRgba(gray, colorTableById('gray'));
    expect(gray).toEqual(pixels);
    colorizeRgba(pixels, colorTableById('viridis'));
    expect(Array.from(pixels.subarray(0, 8))).toEqual([68, 1, 84, 255, 253, 231, 37, 255]);
    expect(Array.from(pixels.subarray(8, 12))).toEqual([...colorAt(colorTableById('viridis'), 100), 200]);
  });

  it('builds a CSS gradient from black to white for gray', () => {
    const gradient = cssGradient(colorTableById('gray'), 3);
    expect(gradient).toBe('linear-gradient(to right, rgb(0, 0, 0) 0%, rgb(128, 128, 128) 50%, rgb(255, 255, 255) 100%)');
  });
});
