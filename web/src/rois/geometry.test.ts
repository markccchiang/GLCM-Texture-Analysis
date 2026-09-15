import type { PolygonShape } from '@glcm/api';
import { describe, expect, it } from 'vitest';
import {
  cutEdges,
  ellipseFromDrag,
  freehandFromPath,
  hasCuts,
  insertVertex,
  isDrawableShape,
  moveVertex,
  nearestEdge,
  rectangleFromDrag,
  removeVertex,
  roiColor,
  shapeBounds,
  shapeKind,
  simplifyPolyline,
  translateShape,
  unionBounds,
} from './geometry';

describe('drawing', () => {
  it('normalizes dragged rectangles in any direction', () => {
    expect(rectangleFromDrag({ x: 10, y: 20 }, { x: 4, y: 25 })).toEqual({ type: 'rectangle', x: 4, y: 20, width: 6, height: 5 });
    expect(rectangleFromDrag({ x: 10, y: 20 }, { x: 4, y: 25 }, true)).toEqual({ type: 'rectangle', x: 4, y: 20, width: 6, height: 6 });
    expect(rectangleFromDrag({ x: 0, y: 0 }, { x: -2, y: -8 }, true)).toEqual({ type: 'rectangle', x: -8, y: -8, width: 8, height: 8 });
  });

  it('inscribes ellipses and circles in the dragged box', () => {
    expect(ellipseFromDrag({ x: 0, y: 0 }, { x: 10, y: 4 })).toEqual({ type: 'ellipse', cx: 5, cy: 2, rx: 5, ry: 2, angle: 0 });
    expect(ellipseFromDrag({ x: 0, y: 0 }, { x: 10, y: 4 }, true)).toMatchObject({ cx: 5, cy: 5, rx: 5, ry: 5 });
  });

  it('rejects degenerate shapes', () => {
    expect(isDrawableShape(rectangleFromDrag({ x: 0, y: 0 }, { x: 0.5, y: 10 }))).toBe(false);
    expect(isDrawableShape(rectangleFromDrag({ x: 0, y: 0 }, { x: 2, y: 2 }))).toBe(true);
    expect(isDrawableShape({ type: 'polygon', points: [[0, 0], [1, 1]] })).toBe(false);
  });

  it('cycles colors and names shape kinds', () => {
    expect(roiColor(0)).toBe(roiColor(8));
    expect(roiColor(1)).not.toBe(roiColor(0));
    expect(shapeKind({ type: 'polygon', points: [], freehand: true })).toBe('freehand');
    expect(shapeKind({ type: 'polygon', points: [] })).toBe('polygon');
  });
});

describe('bounds', () => {
  it('covers rotated ellipses', () => {
    expect(shapeBounds({ type: 'ellipse', cx: 10, cy: 10, rx: 4, ry: 2, angle: 90 })).toEqual(
      expect.objectContaining({ width: expect.closeTo(4, 10), height: expect.closeTo(8, 10) }),
    );
    const diagonal = shapeBounds({ type: 'ellipse', cx: 0, cy: 0, rx: 2, ry: 2, angle: 45 });
    expect(diagonal.width).toBeCloseTo(4, 10);
  });

  it('handles rectangles with negative size and polygons', () => {
    expect(shapeBounds({ type: 'rectangle', x: 10, y: 10, width: -4, height: 2 })).toEqual({ x: 6, y: 10, width: 4, height: 2 });
    expect(shapeBounds({ type: 'polygon', points: [[1, 5], [4, 2], [3, 9]] })).toEqual({ x: 1, y: 2, width: 3, height: 7 });
    expect(unionBounds([{ x: 0, y: 0, width: 2, height: 2 }, { x: 5, y: -1, width: 1, height: 1 }])).toEqual({ x: 0, y: -1, width: 6, height: 3 });
    expect(unionBounds([])).toBeNull();
  });

  it('translates every shape type', () => {
    expect(translateShape({ type: 'rectangle', x: 1, y: 2, width: 3, height: 4 }, 1, -1)).toMatchObject({ x: 2, y: 1 });
    expect(translateShape({ type: 'ellipse', cx: 1, cy: 2, rx: 3, ry: 4 }, 1, -1)).toMatchObject({ cx: 2, cy: 1 });
    expect(translateShape({ type: 'polygon', points: [[0, 0], [1, 1]] }, 1, -1)).toMatchObject({ points: [[1, -1], [2, 0]] });
  });
});

describe('polygons', () => {
  const square: PolygonShape = { type: 'polygon', points: [[0, 0], [10, 0], [10, 10], [0, 10]] };

  it('simplifies traced paths with Douglas–Peucker', () => {
    const path: Array<[number, number]> = [[0, 0], [1, 0.1], [2, -0.2], [3, 0.3], [4, 0], [4, 4]];
    expect(simplifyPolyline(path, 0.5)).toEqual([[0, 0], [4, 0], [4, 4]]);
    expect(simplifyPolyline(path, 0.05)).toHaveLength(6);
    expect(freehandFromPath(path)).toEqual({ type: 'polygon', points: [[0, 0], [4, 0], [4, 4]], freehand: true });
  });

  it('inserts, moves and removes vertices', () => {
    expect(nearestEdge(square.points, { x: 10.5, y: 5 })).toBe(1);
    expect(nearestEdge(square.points, { x: -1, y: 5 })).toBe(3);
    expect(insertVertex(square, { x: 5, y: 10 }).points).toEqual([[0, 0], [10, 0], [10, 10], [5, 10], [0, 10]]);
    expect(moveVertex(square, 2, { x: 12, y: 12 }).points[2]).toEqual([12, 12]);
    expect(removeVertex(square, 0).points).toEqual([[10, 0], [10, 10], [0, 10]]);
    const triangle = removeVertex(square, 0);
    expect(removeVertex(triangle, 0)).toBe(triangle);
  });
});

describe('cut edges', () => {
  it('marks the segments a polygon runs both ways', () => {
    // A square with a square hole, joined by a cut from (0, 0) to (2, 2) and back
    const points: Array<[number, number]> = [
      [0, 0],
      [6, 0],
      [6, 6],
      [0, 6],
      [0, 0],
      [2, 2],
      [4, 2],
      [4, 4],
      [2, 4],
      [2, 2],
    ];
    expect(cutEdges(points)).toEqual([false, false, false, false, true, false, false, false, false, true]);
    expect(hasCuts(points)).toBe(true);
  });

  it('finds no cuts in ordinary polygons, including repeated vertices', () => {
    expect(
      hasCuts([
        [0, 0],
        [4, 0],
        [4, 4],
        [4, 4],
        [0, 4],
      ]),
    ).toBe(false);
    expect(cutEdges([[1, 1]])).toEqual([false]);
  });
});
