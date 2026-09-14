import { describe, expect, it } from 'vitest';
import {
  MAX_SCALE,
  MIN_SCALE,
  centreOn,
  fitToView,
  imageFits,
  imageToScreen,
  nextZoomStep,
  pan,
  pixelAt,
  screenToImage,
  visibleRect,
  zoomAt,
  zoomTo,
  zoomToRect,
  type Viewport,
} from './viewport';

const viewport: Viewport = { scale: 2, x: 30, y: -10 };

describe('coordinate conversion', () => {
  it('maps screen and image coordinates both ways', () => {
    const image = screenToImage(viewport, { x: 50, y: 90 });
    expect(image).toEqual({ x: 10, y: 50 });
    expect(imageToScreen(viewport, image)).toEqual({ x: 50, y: 90 });
  });

  it('finds the pixel under a point, using [c, c + 1) pixel squares', () => {
    const image = { width: 20, height: 60 };
    expect(pixelAt(viewport, { x: 50, y: 90 }, image)).toEqual({ x: 10, y: 50 });
    expect(pixelAt(viewport, { x: 51.9, y: 91.9 }, image)).toEqual({ x: 10, y: 50 });
    expect(pixelAt(viewport, { x: 29.9, y: 0 }, image)).toBeNull();
    expect(pixelAt(viewport, { x: 70, y: 0 }, image)).toBeNull(); // x = 20 is past the last column
  });
});

describe('zoom', () => {
  it('keeps the image point under the anchor fixed', () => {
    const anchor = { x: 123, y: 77 };
    const before = screenToImage(viewport, anchor);
    for (const factor of [0.5, 1.25, 3]) {
      const after = screenToImage(zoomAt(viewport, factor, anchor), anchor);
      expect(after.x).toBeCloseTo(before.x, 10);
      expect(after.y).toBeCloseTo(before.y, 10);
    }
  });

  it('clamps the scale to 5 %–3200 %', () => {
    expect(zoomAt(viewport, 1000, { x: 0, y: 0 }).scale).toBe(MAX_SCALE);
    expect(zoomAt(viewport, 1e-6, { x: 0, y: 0 }).scale).toBe(MIN_SCALE);
    expect(zoomTo(viewport, 1, { x: 0, y: 0 }).scale).toBe(1);
  });

  it('steps through the zoom levels', () => {
    expect(nextZoomStep(1, 1)).toBe(1.5);
    expect(nextZoomStep(1, -1)).toBeCloseTo(2 / 3, 10);
    expect(nextZoomStep(1.2, 1)).toBe(1.5);
    expect(nextZoomStep(1.2, -1)).toBe(1);
    expect(nextZoomStep(MAX_SCALE, 1)).toBe(MAX_SCALE);
    expect(nextZoomStep(MIN_SCALE, -1)).toBe(MIN_SCALE);
  });
});

describe('fit, zoom to rectangle and centring', () => {
  it('fits the whole image with a margin and centres it', () => {
    const fitted = fitToView({ width: 400, height: 200 }, { width: 832, height: 632 });
    expect(fitted.scale).toBe(2); // (832 - 32) / 400
    expect(fitted.x).toBe(16);
    expect(fitted.y).toBe(116); // (632 - 400) / 2
    expect(imageFits(fitted, { width: 400, height: 200 }, { width: 832, height: 632 })).toBe(true);
    expect(imageFits(zoomAt(fitted, 2, { x: 0, y: 0 }), { width: 400, height: 200 }, { width: 832, height: 632 })).toBe(false);
  });

  it('frames a rectangle with 10 % padding', () => {
    const view = { width: 600, height: 600 };
    const framed = zoomToRect({ x: 100, y: 50, width: 50, height: 25 }, view);
    expect(framed.scale).toBe(10); // 600 / (50 * 1.2)
    const centre = imageToScreen(framed, { x: 125, y: 62.5 });
    expect(centre.x).toBeCloseTo(300, 10);
    expect(centre.y).toBeCloseTo(300, 10);
  });

  it('centres an image point and reports the visible area', () => {
    const view = { width: 200, height: 100 };
    const centred = centreOn(viewport, { x: 40, y: 20 }, view);
    expect(imageToScreen(centred, { x: 40, y: 20 })).toEqual({ x: 100, y: 50 });
    expect(visibleRect(centred, view)).toEqual({ x: -10, y: -5, width: 100, height: 50 });
    expect(pan(centred, 10, -5)).toEqual({ scale: 2, x: centred.x + 10, y: centred.y - 5 });
  });
});
