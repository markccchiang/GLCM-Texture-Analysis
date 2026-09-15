import { describe, expect, it } from 'vitest';
import { areaMm2, formatArea, formatLength, formatSpacing, isAnisotropic, offsetLengthsMm, sameSpacing, scaleBar } from './spacing';

describe('pixel spacing', () => {
  it('compares spacings, including missing ones', () => {
    expect(sameSpacing({ x: 1, y: 2 }, { x: 1, y: 2 })).toBe(true);
    expect(sameSpacing({ x: 1, y: 2 }, { x: 2, y: 1 })).toBe(false);
    expect(sameSpacing(null, undefined)).toBe(true);
    expect(sameSpacing(null, { x: 1, y: 1 })).toBe(false);
  });

  it('tells square from non-square pixels', () => {
    expect(isAnisotropic({ x: 1, y: 1000 / 750 })).toBe(true);
    expect(isAnisotropic({ x: 0.41135, y: 0.41152 })).toBe(false);
  });

  it('computes and formats areas and lengths', () => {
    expect(areaMm2(7548, { x: 0.5, y: 0.25 })).toBe(943.5);
    expect(formatArea(943.5)).toBe('944 mm²');
    expect(formatArea(0.01234)).toBe('0.0123 mm²');
    expect(formatArea(12_345.6)).toBe(`${(12_346).toLocaleString()} mm²`);
    expect(formatLength(0.5)).toBe('500 µm');
    expect(formatLength(20)).toBe('20 mm');
    expect(formatLength(2000)).toBe('2 m');
    expect(formatSpacing({ x: 0.703125, y: 0.703125 })).toBe('0.703125 mm');
    expect(formatSpacing({ x: 1, y: 1000 / 750 })).toBe('1 × 1.33333 mm');
  });

  it('chooses a 1, 2 or 5 scale bar between half and all of the maximum width', () => {
    // 0.1 mm per screen pixel: 150 px would be 15 mm, so 10 mm at 100 px
    expect(scaleBar(0.1)).toEqual({ lengthMm: 10, widthPx: 100, label: '10 mm' });
    expect(scaleBar(0.02)).toEqual({ lengthMm: 2, widthPx: 100, label: '2 mm' });
    expect(scaleBar(0.004)?.label).toBe('500 µm');
    for (const mmPerPixel of [0.0007, 0.013, 0.29, 3.3, 71]) {
      const bar = scaleBar(mmPerPixel)!;
      expect(bar.widthPx).toBeGreaterThanOrEqual(60);
      expect(bar.widthPx).toBeLessThanOrEqual(150);
    }
    expect(scaleBar(0)).toBeNull();
  });

  it('gives the physical offsets of a distance per direction', () => {
    expect(offsetLengthsMm({ x: 3, y: 4 }, 2)).toEqual({ horizontal: 6, vertical: 8, diagonal: 10 });
  });
});
