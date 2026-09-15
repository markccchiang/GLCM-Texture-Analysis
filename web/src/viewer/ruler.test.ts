import { describe, expect, it } from 'vitest';
import { formatRuler, measureRuler, snapRuler } from './ruler';

describe('ruler', () => {
  it('measures the length and the angle counter-clockwise from the horizontal', () => {
    // 60 right and 80 down: 100 px, pointing below the horizontal
    const line = { start: { x: 10, y: 20 }, end: { x: 70, y: 100 } };
    const plain = measureRuler(line, null);
    expect(plain.lengthPx).toBe(100);
    expect(plain.lengthMm).toBeNull();
    expect(plain.angle).toBeCloseTo((Math.atan2(-80, 60) * 180) / Math.PI, 12);
    expect(formatRuler(plain)).toBe('100.0 px · -53.1°');
  });

  it('uses the pixel width and height for the length in mm and the angle on the object', () => {
    const line = { start: { x: 10, y: 20 }, end: { x: 70, y: 100 } };
    const spaced = measureRuler(line, { x: 0.5, y: 0.25 });
    // 30 mm across and 20 mm down
    expect(spaced.lengthMm).toBeCloseTo(Math.hypot(30, 20), 12);
    expect(spaced.angle).toBeCloseTo((Math.atan2(-20, 30) * 180) / Math.PI, 12);
    expect(formatRuler(spaced)).toBe('100.0 px · 36.06 mm · -33.7°');
  });

  it('reports a line to the left as 180° and an empty line as 0°', () => {
    expect(measureRuler({ start: { x: 10, y: 10 }, end: { x: 0, y: 10 } }, null).angle).toBe(180);
    expect(measureRuler({ start: { x: 5, y: 5 }, end: { x: 5, y: 5 } }, null)).toEqual({ lengthPx: 0, lengthMm: null, angle: 0 });
  });

  it('snaps the end to the nearest 45° keeping the distance', () => {
    const horizontal = snapRuler({ x: 0, y: 0 }, { x: 10, y: 1 });
    expect(horizontal.x).toBeCloseTo(Math.hypot(10, 1), 12);
    expect(horizontal.y).toBeCloseTo(0, 12);
    const diagonal = snapRuler({ x: 0, y: 0 }, { x: 10, y: 9 });
    const length = Math.hypot(10, 9);
    expect(diagonal.x).toBeCloseTo(length / Math.SQRT2, 12);
    expect(diagonal.y).toBeCloseTo(length / Math.SQRT2, 12);
    expect(snapRuler({ x: 3, y: 3 }, { x: 3, y: 3 })).toEqual({ x: 3, y: 3 });
  });
});
