// The ruler tool: one line on the image, measured in pixels and, with a pixel spacing, in millimetres

import type { PixelSpacing } from '@glcm/api';
import { formatLength } from '../image/spacing';
import type { Point } from './viewport';

/** End points in image coordinates */
export interface RulerLine {
  start: Point;
  end: Point;
}

export interface RulerMeasurement {
  lengthPx: number;
  /** null without a pixel spacing */
  lengthMm: number | null;
  /**
   * Degrees counter-clockwise from the horizontal, in (-180, 180]. The image's y axis points down; with a pixel spacing
   * the angle is the one on the object, which differs for non-square pixels.
   */
  angle: number;
}

export function measureRuler({ start, end }: RulerLine, spacing: PixelSpacing | null): RulerMeasurement {
  const dx = end.x - start.x;
  const dy = end.y - start.y;
  const lengthPx = Math.hypot(dx, dy);
  const physicalX = spacing ? dx * spacing.x : dx;
  const physicalY = spacing ? dy * spacing.y : dy;
  const angle = lengthPx === 0 ? 0 : (Math.atan2(-physicalY, physicalX) * 180) / Math.PI;
  return { lengthPx, lengthMm: spacing ? Math.hypot(physicalX, physicalY) : null, angle: angle <= -180 ? 180 : angle };
}

/** The end point at the same distance from the start, moved to the nearest multiple of 45° (Shift while drawing) */
export function snapRuler(start: Point, end: Point): Point {
  const dx = end.x - start.x;
  const dy = end.y - start.y;
  const length = Math.hypot(dx, dy);
  if (length === 0) {
    return end;
  }
  const step = Math.PI / 4;
  const angle = Math.round(Math.atan2(dy, dx) / step) * step;
  return { x: start.x + length * Math.cos(angle), y: start.y + length * Math.sin(angle) };
}

/** e.g. "100.0 px · 50 mm · -53.1°" */
export function formatRuler(measurement: RulerMeasurement): string {
  const parts = [`${measurement.lengthPx.toFixed(1)} px`];
  if (measurement.lengthMm !== null) {
    parts.push(formatLength(measurement.lengthMm));
  }
  parts.push(`${measurement.angle.toFixed(1)}°`);
  return parts.join(' · ');
}
