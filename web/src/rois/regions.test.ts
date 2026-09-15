import { describe, expect, it } from 'vitest';
import { defaultWandTolerance, regionShape } from './regions';

describe('regions', () => {
  it('keeps outlines that fit exactly', () => {
    const points: Array<[number, number]> = [
      [1, 1],
      [5, 1],
      [5, 5],
      [1, 5],
    ];
    expect(regionShape({ points })).toEqual({ shape: { type: 'polygon', points }, simplified: false });
  });

  it('simplifies outlines with too many vertices until they fit', () => {
    // A staircase circle of radius 40 along pixel edges: many small steps
    const points: Array<[number, number]> = [];
    for (let i = 0; i < 400; i += 1) {
      const angle = (i / 400) * 2 * Math.PI;
      const x = Math.round(50 + 40 * Math.cos(angle));
      const y = Math.round(50 + 40 * Math.sin(angle));
      points.push([x, y], [x + 1, y]);
    }
    const { shape, simplified } = regionShape({ points }, 100);
    expect(simplified).toBe(true);
    expect(shape.points.length).toBeLessThanOrEqual(100);
    expect(shape.points.length).toBeGreaterThanOrEqual(3);
    expect(shape.points[0]).toEqual(points[0]);
    // Every kept vertex is one of the original ones
    const originals = new Set(points.map(([x, y]) => `${x},${y}`));
    expect(shape.points.every(([x, y]) => originals.has(`${x},${y}`))).toBe(true);
  });

  it('uses 5 % of the display window as the default wand tolerance', () => {
    expect(defaultWandTolerance(0, 255)).toBe(13);
    expect(defaultWandTolerance(1000, 1020)).toBe(1);
    expect(defaultWandTolerance(7, 7)).toBe(1);
    expect(defaultWandTolerance(0, 4095)).toBe(205);
  });
});
