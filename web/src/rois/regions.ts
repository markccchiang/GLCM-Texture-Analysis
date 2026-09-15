// ROIs from pixel values: the regions found by the threshold and magic wand endpoints become polygon ROIs.

import { MAX_POLYGON_VERTICES, type PolygonShape, type SelectedRegion } from '@glcm/api';
import { simplifyPolyline } from './geometry';

/** Regions smaller than this (pixels, holes included) are left out by Threshold ROI by default */
export const DEFAULT_THRESHOLD_MIN_PIXELS = 50;

/** Share of the display window used as the default magic wand tolerance */
const WAND_TOLERANCE_SHARE = 0.05;

export interface RegionShape {
  shape: PolygonShape;
  /** The exact outline had more vertices than an ROI may have and was simplified */
  simplified: boolean;
}

/**
 * The polygon ROI of a region. The outline follows the pixel edges, so its pixels are exactly the region's; an outline
 * with more than maxVertices vertices is simplified with a growing Douglas–Peucker tolerance until it fits, which changes
 * its pixels slightly.
 */
export function regionShape(region: Pick<SelectedRegion, 'points'>, maxVertices = MAX_POLYGON_VERTICES): RegionShape {
  const points = region.points.map(([x, y]): [number, number] => [x, y]);
  if (points.length <= maxVertices) {
    return { shape: { type: 'polygon', points }, simplified: false };
  }
  // Closing the ring keeps the first vertex at both ends of the polyline
  const ring = [...points, points[0]];
  let simplified = points;
  for (let tolerance = 0.5; simplified.length > maxVertices; tolerance *= 2) {
    simplified = simplifyPolyline(ring, tolerance).slice(0, -1);
  }
  return { shape: { type: 'polygon', points: simplified }, simplified: true };
}

/** 5 % of the display window, at least 1 */
export function defaultWandTolerance(windowMin: number, windowMax: number): number {
  return Math.max(1, Math.round((windowMax - windowMin) * WAND_TOLERANCE_SHARE));
}
