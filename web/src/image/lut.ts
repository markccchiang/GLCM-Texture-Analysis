// Lookup-table window/level rendering: the fallback renderer when WebGL2 is unavailable, and the reference for the
// WebGL2 shader (doc/ui-design-plan.md, section 6.1).

import { windowLevel } from '@glcm/api';
import type { RawImage } from './raw';

/** Display value for every possible sample value (256 or 65,536 entries) */
export function buildLut(bitDepth: 8 | 16, windowMin: number, windowMax: number): Uint8Array {
  const lut = new Uint8Array(bitDepth === 16 ? 65536 : 256);
  for (let value = 0; value < lut.length; value += 1) {
    lut[value] = windowLevel(value, windowMin, windowMax);
  }
  return lut;
}

/** Opaque gray RGBA pixels (e.g. for ImageData) of the whole image */
export function renderToRgba(image: RawImage, windowMin: number, windowMax: number, output?: Uint8ClampedArray): Uint8ClampedArray {
  const pixels = image.width * image.height;
  const rgba = output ?? new Uint8ClampedArray(pixels * 4);
  if (rgba.length !== pixels * 4) {
    throw new Error(`Output has ${rgba.length} bytes, expected ${pixels * 4}`);
  }

  const lut = buildLut(image.bitDepth, windowMin, windowMax);
  const samples = image.samples;
  for (let i = 0, j = 0; i < pixels; i += 1, j += 4) {
    const gray = lut[samples[i]];
    rgba[j] = gray;
    rgba[j + 1] = gray;
    rgba[j + 2] = gray;
    rgba[j + 3] = 255;
  }
  return rgba;
}
