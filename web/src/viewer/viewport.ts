// Viewport maths for the image canvas (doc/ui-design-plan.md, section 6.1).
//
// A viewport maps image coordinates to screen coordinates: screen = image * scale + (x, y). Pixel (column c, row r)
// covers [c, c + 1) x [r, r + 1) in image coordinates, as in core/roi/Roi.

export interface Point {
  x: number;
  y: number;
}

export interface Size {
  width: number;
  height: number;
}

export interface Rect extends Point, Size {}

export interface Viewport {
  scale: number;
  x: number;
  y: number;
}

export const MIN_SCALE = 0.05;
export const MAX_SCALE = 32;

/** Zoom levels used by the zoom in/out buttons and keys */
export const ZOOM_STEPS = [0.05, 0.1, 0.125, 0.25, 1 / 3, 0.5, 2 / 3, 1, 1.5, 2, 3, 4, 6, 8, 12, 16, 24, 32];

export function clampScale(scale: number): number {
  return Math.min(MAX_SCALE, Math.max(MIN_SCALE, scale));
}

export function screenToImage(viewport: Viewport, point: Point): Point {
  return { x: (point.x - viewport.x) / viewport.scale, y: (point.y - viewport.y) / viewport.scale };
}

export function imageToScreen(viewport: Viewport, point: Point): Point {
  return { x: point.x * viewport.scale + viewport.x, y: point.y * viewport.scale + viewport.y };
}

/** Pixel under a screen point, or null outside the image */
export function pixelAt(viewport: Viewport, point: Point, image: Size): Point | null {
  const position = screenToImage(viewport, point);
  const x = Math.floor(position.x);
  const y = Math.floor(position.y);
  return x >= 0 && y >= 0 && x < image.width && y < image.height ? { x, y } : null;
}

/** Zooms by a factor while keeping the image point under the anchor (screen coordinates) fixed */
export function zoomAt(viewport: Viewport, factor: number, anchor: Point): Viewport {
  const scale = clampScale(viewport.scale * factor);
  const ratio = scale / viewport.scale;
  return {
    scale,
    x: anchor.x - (anchor.x - viewport.x) * ratio,
    y: anchor.y - (anchor.y - viewport.y) * ratio,
  };
}

export function zoomTo(viewport: Viewport, scale: number, anchor: Point): Viewport {
  return zoomAt(viewport, scale / viewport.scale, anchor);
}

/** Next zoom step above (direction 1) or below (direction -1) the current scale */
export function nextZoomStep(scale: number, direction: 1 | -1): number {
  const epsilon = 1e-9;
  if (direction > 0) {
    return ZOOM_STEPS.find((step) => step > scale * (1 + epsilon)) ?? MAX_SCALE;
  }
  return [...ZOOM_STEPS].reverse().find((step) => step < scale * (1 - epsilon)) ?? MIN_SCALE;
}

export function pan(viewport: Viewport, dx: number, dy: number): Viewport {
  return { ...viewport, x: viewport.x + dx, y: viewport.y + dy };
}

/** Largest scale within the limits that shows the whole image with a margin, centred */
export function fitToView(image: Size, view: Size, margin = 16): Viewport {
  const availableWidth = Math.max(1, view.width - 2 * margin);
  const availableHeight = Math.max(1, view.height - 2 * margin);
  const scale = clampScale(Math.min(availableWidth / image.width, availableHeight / image.height));
  return {
    scale,
    x: (view.width - image.width * scale) / 2,
    y: (view.height - image.height * scale) / 2,
  };
}

/** Shows a rectangle (image coordinates) with 10 % padding on each side, centred */
export function zoomToRect(rect: Rect, view: Size, padding = 0.1): Viewport {
  const width = Math.max(rect.width, 1) * (1 + 2 * padding);
  const height = Math.max(rect.height, 1) * (1 + 2 * padding);
  const scale = clampScale(Math.min(view.width / width, view.height / height));
  const centre = { x: rect.x + rect.width / 2, y: rect.y + rect.height / 2 };
  return { scale, x: view.width / 2 - centre.x * scale, y: view.height / 2 - centre.y * scale };
}

/** Moves the viewport so that an image point is at the centre of the view */
export function centreOn(viewport: Viewport, point: Point, view: Size): Viewport {
  return { ...viewport, x: view.width / 2 - point.x * viewport.scale, y: view.height / 2 - point.y * viewport.scale };
}

/** The part of the image coordinate space visible in the view */
export function visibleRect(viewport: Viewport, view: Size): Rect {
  const topLeft = screenToImage(viewport, { x: 0, y: 0 });
  return { x: topLeft.x, y: topLeft.y, width: view.width / viewport.scale, height: view.height / viewport.scale };
}

/** Whether the whole image fits inside the view at the current scale */
export function imageFits(viewport: Viewport, image: Size, view: Size): boolean {
  return image.width * viewport.scale <= view.width && image.height * viewport.scale <= view.height;
}
