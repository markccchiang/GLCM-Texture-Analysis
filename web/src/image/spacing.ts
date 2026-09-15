// Pixel spacing: millimetres per pixel of the open image, for the scale bar, ROI areas in mm² and the note about
// non-square pixels. GLCM features are computed in pixels whatever the spacing.

import type { PixelSpacing } from '@glcm/api';

const tidy = (value: number) => Number(value.toPrecision(12));

export function sameSpacing(a: PixelSpacing | null | undefined, b: PixelSpacing | null | undefined): boolean {
  if (!a || !b) {
    return (a ?? null) === (b ?? null);
  }
  return a.x === b.x && a.y === b.y;
}

/** Pixels whose width and height differ by more than 0.1 % */
export function isAnisotropic(spacing: PixelSpacing): boolean {
  return Math.abs(spacing.x - spacing.y) > 1e-3 * Math.max(spacing.x, spacing.y);
}

/** Same product as the core's areaMm2 CSV column */
export function areaMm2(pixelCount: number, spacing: PixelSpacing): number {
  return pixelCount * spacing.x * spacing.y;
}

const significant = (value: number, digits: number) => String(Number(value.toPrecision(digits)));

/** µm below 1 mm, m from 1000 mm, otherwise mm */
export function formatLength(mm: number): string {
  if (mm < 1) {
    return `${significant(mm * 1000, 4)} µm`;
  }
  if (mm >= 1000) {
    return `${significant(mm / 1000, 4)} m`;
  }
  return `${significant(mm, 4)} mm`;
}

export function formatArea(mm2: number): string {
  return mm2 >= 1000 ? `${Math.round(mm2).toLocaleString()} mm²` : `${significant(mm2, 3)} mm²`;
}

export function formatSpacing(spacing: PixelSpacing): string {
  return spacing.x === spacing.y ? `${significant(spacing.x, 6)} mm` : `${significant(spacing.x, 6)} × ${significant(spacing.y, 6)} mm`;
}

export interface ScaleBar {
  lengthMm: number;
  widthPx: number;
  label: string;
}

/** The longest 1, 2 or 5 × 10^k mm that fits into maxWidthPx screen pixels (so between half and all of it) */
export function scaleBar(mmPerScreenPixel: number, maxWidthPx = 150): ScaleBar | null {
  if (!Number.isFinite(mmPerScreenPixel) || mmPerScreenPixel <= 0) {
    return null;
  }
  const longest = maxWidthPx * mmPerScreenPixel;
  const power = 10 ** Math.floor(Math.log10(longest));
  const lengthMm = tidy([5, 2, 1].map((multiple) => multiple * power).find((candidate) => candidate <= longest * (1 + 1e-9))!);
  return { lengthMm, widthPx: lengthMm / mmPerScreenPixel, label: formatLength(lengthMm) };
}

/** Physical distance between a pixel and its neighbour at distance d in each direction */
export function offsetLengthsMm(spacing: PixelSpacing, distance: number): { horizontal: number; vertical: number; diagonal: number } {
  return { horizontal: distance * spacing.x, vertical: distance * spacing.y, diagonal: distance * Math.hypot(spacing.x, spacing.y) };
}
