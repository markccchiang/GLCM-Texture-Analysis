// ROI geometry in image pixel coordinates (doc/ui-design-plan.md, section 6.2). Masks are computed only by glcm_core;
// these helpers create and edit shapes and compute bounds for zooming and labels.

import type { EllipseShape, PolygonShape, RectangleShape, RoiShape } from '@glcm/api';
import type { Point, Rect } from '../viewer/viewport';

export type ShapeKind = 'rectangle' | 'ellipse' | 'polygon' | 'freehand';

/** Colors given to new ROIs in turn; chosen to stand out on grayscale images */
export const ROI_COLORS = ['#FFD400', '#00C2FF', '#FF4FD8', '#39FF6A', '#FF8A00', '#B388FF', '#FF3B3B', '#00FFD1'];

export function roiColor(index: number): string {
  return ROI_COLORS[((index % ROI_COLORS.length) + ROI_COLORS.length) % ROI_COLORS.length];
}

export function shapeKind(shape: RoiShape): ShapeKind {
  return shape.type === 'polygon' && shape.freehand ? 'freehand' : shape.type;
}

export const SHAPE_LABELS: Record<ShapeKind, string> = {
  rectangle: 'Rectangle',
  ellipse: 'Ellipse',
  polygon: 'Polygon',
  freehand: 'Freehand',
};

/** Rectangle dragged from start to end; Shift makes it a square in the drag direction */
export function rectangleFromDrag(start: Point, end: Point, square = false): RectangleShape {
  let dx = end.x - start.x;
  let dy = end.y - start.y;
  if (square) {
    const side = Math.max(Math.abs(dx), Math.abs(dy));
    dx = Math.sign(dx || 1) * side;
    dy = Math.sign(dy || 1) * side;
  }
  return {
    type: 'rectangle',
    x: Math.min(start.x, start.x + dx),
    y: Math.min(start.y, start.y + dy),
    width: Math.abs(dx),
    height: Math.abs(dy),
  };
}

/** Ellipse inscribed in the dragged bounding box; Shift makes it a circle */
export function ellipseFromDrag(start: Point, end: Point, circle = false): EllipseShape {
  const box = rectangleFromDrag(start, end, circle);
  return { type: 'ellipse', cx: box.x + box.width / 2, cy: box.y + box.height / 2, rx: box.width / 2, ry: box.height / 2, angle: 0 };
}

/** Axis-aligned bounds of a shape */
export function shapeBounds(shape: RoiShape): Rect {
  switch (shape.type) {
    case 'rectangle': {
      const x = Math.min(shape.x, shape.x + shape.width);
      const y = Math.min(shape.y, shape.y + shape.height);
      return { x, y, width: Math.abs(shape.width), height: Math.abs(shape.height) };
    }
    case 'ellipse': {
      const angle = ((shape.angle ?? 0) * Math.PI) / 180;
      const cos = Math.cos(angle);
      const sin = Math.sin(angle);
      const halfWidth = Math.hypot(shape.rx * cos, shape.ry * sin);
      const halfHeight = Math.hypot(shape.rx * sin, shape.ry * cos);
      return { x: shape.cx - halfWidth, y: shape.cy - halfHeight, width: 2 * halfWidth, height: 2 * halfHeight };
    }
    case 'polygon': {
      if (shape.points.length === 0) {
        return { x: 0, y: 0, width: 0, height: 0 };
      }
      const xs = shape.points.map(([x]) => x);
      const ys = shape.points.map(([, y]) => y);
      const x = Math.min(...xs);
      const y = Math.min(...ys);
      return { x, y, width: Math.max(...xs) - x, height: Math.max(...ys) - y };
    }
  }
}

/** Smallest rectangle containing all rectangles, or null for none */
export function unionBounds(rects: readonly Rect[]): Rect | null {
  if (rects.length === 0) {
    return null;
  }
  const left = Math.min(...rects.map((r) => r.x));
  const top = Math.min(...rects.map((r) => r.y));
  const right = Math.max(...rects.map((r) => r.x + r.width));
  const bottom = Math.max(...rects.map((r) => r.y + r.height));
  return { x: left, y: top, width: right - left, height: bottom - top };
}

export function translateShape(shape: RoiShape, dx: number, dy: number): RoiShape {
  switch (shape.type) {
    case 'rectangle':
      return { ...shape, x: shape.x + dx, y: shape.y + dy };
    case 'ellipse':
      return { ...shape, cx: shape.cx + dx, cy: shape.cy + dy };
    case 'polygon':
      return { ...shape, points: shape.points.map(([x, y]) => [x + dx, y + dy] as [number, number]) };
  }
}

function distanceToSegment(point: Point, a: Point, b: Point): number {
  const dx = b.x - a.x;
  const dy = b.y - a.y;
  const lengthSquared = dx * dx + dy * dy;
  const t = lengthSquared === 0 ? 0 : Math.max(0, Math.min(1, ((point.x - a.x) * dx + (point.y - a.y) * dy) / lengthSquared));
  return Math.hypot(point.x - (a.x + t * dx), point.y - (a.y + t * dy));
}

/** Douglas–Peucker simplification of an open polyline; the end points are always kept */
export function simplifyPolyline(points: ReadonlyArray<readonly [number, number]>, tolerance: number): Array<[number, number]> {
  if (points.length <= 2) {
    return points.map(([x, y]) => [x, y]);
  }
  const keep = new Uint8Array(points.length);
  keep[0] = 1;
  keep[points.length - 1] = 1;
  const stack: Array<[number, number]> = [[0, points.length - 1]];
  while (stack.length > 0) {
    const [first, last] = stack.pop()!;
    const a = { x: points[first][0], y: points[first][1] };
    const b = { x: points[last][0], y: points[last][1] };
    let farthest = -1;
    let farthestDistance = tolerance;
    for (let i = first + 1; i < last; i += 1) {
      const distance = distanceToSegment({ x: points[i][0], y: points[i][1] }, a, b);
      if (distance > farthestDistance) {
        farthest = i;
        farthestDistance = distance;
      }
    }
    if (farthest >= 0) {
      keep[farthest] = 1;
      stack.push([first, farthest], [farthest, last]);
    }
  }
  return points.filter((_, i) => keep[i] === 1).map(([x, y]) => [x, y]);
}

/** Freehand polygon from a traced path, simplified with a 0.5 px tolerance */
export function freehandFromPath(path: ReadonlyArray<readonly [number, number]>): PolygonShape {
  return { type: 'polygon', points: simplifyPolyline(path, 0.5), freehand: true };
}

/** Index of the polygon edge (from vertex i to i + 1, wrapping) closest to a point */
export function nearestEdge(points: ReadonlyArray<readonly [number, number]>, point: Point): number {
  let best = 0;
  let bestDistance = Infinity;
  points.forEach(([x, y], i) => {
    const [nx, ny] = points[(i + 1) % points.length];
    const distance = distanceToSegment(point, { x, y }, { x: nx, y: ny });
    if (distance < bestDistance) {
      best = i;
      bestDistance = distance;
    }
  });
  return best;
}

/** Adds a vertex at a point on the nearest edge */
export function insertVertex(shape: PolygonShape, point: Point): PolygonShape {
  const edge = nearestEdge(shape.points, point);
  const points = [...shape.points];
  points.splice(edge + 1, 0, [point.x, point.y]);
  return { ...shape, points };
}

/** Removes a vertex, keeping at least 3 */
export function removeVertex(shape: PolygonShape, index: number): PolygonShape {
  if (shape.points.length <= 3) {
    return shape;
  }
  return { ...shape, points: shape.points.filter((_, i) => i !== index) };
}

export function moveVertex(shape: PolygonShape, index: number, point: Point): PolygonShape {
  return { ...shape, points: shape.points.map((vertex, i) => (i === index ? [point.x, point.y] : vertex)) };
}

/** A shape large enough to keep: drag shapes need some extent, polygons 3 vertices */
export function isDrawableShape(shape: RoiShape): boolean {
  switch (shape.type) {
    case 'rectangle':
      return Math.abs(shape.width) >= 1 && Math.abs(shape.height) >= 1;
    case 'ellipse':
      return shape.rx >= 0.5 && shape.ry >= 0.5;
    case 'polygon':
      return shape.points.length >= 3;
  }
}

/** Stable text for a shape, used to cache statistics */
export function shapeKey(shape: RoiShape): string {
  return JSON.stringify(shape);
}

/**
 * For each polygon edge (vertex i to i + 1, wrapping), whether it is a cut: the polygon also runs the same segment the
 * other way. The brush, eraser, union and subtract join separate parts and holes into one polygon with such zero-width
 * cuts, which cover no pixels under the even-odd rule and are not drawn. Zero-length edges are not cuts.
 */
export function cutEdges(points: ReadonlyArray<readonly [number, number]>): boolean[] {
  const count = points.length;
  const key = (a: readonly [number, number], b: readonly [number, number]) => `${a[0]},${a[1]};${b[0]},${b[1]}`;
  const edges = new Set<string>();
  points.forEach((point, i) => edges.add(key(point, points[(i + 1) % count])));
  return points.map((point, i) => {
    const next = points[(i + 1) % count];
    const degenerate = point[0] === next[0] && point[1] === next[1];
    return !degenerate && edges.has(key(next, point));
  });
}

/** Whether a polygon joins several parts or holes with cuts; such polygons are changed as a whole, not vertex by vertex */
export function hasCuts(points: ReadonlyArray<readonly [number, number]>): boolean {
  return cutEdges(points).some(Boolean);
}
