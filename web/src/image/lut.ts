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

/** Opaque RGBA pixels (e.g. for ImageData) of the whole image: gray, or coloured with a table of 256 RGB bytes */
export function renderToRgba(
  image: RawImage,
  windowMin: number,
  windowMax: number,
  output?: Uint8ClampedArray,
  table?: Uint8Array,
): Uint8ClampedArray {
  const pixels = image.width * image.height;
  const rgba = output ?? new Uint8ClampedArray(pixels * 4);
  if (rgba.length !== pixels * 4) {
    throw new Error(`Output has ${rgba.length} bytes, expected ${pixels * 4}`);
  }

  const lut = buildLut(image.bitDepth, windowMin, windowMax);
  const samples = image.samples;
  for (let i = 0, j = 0; i < pixels; i += 1, j += 4) {
    const display = lut[samples[i]];
    if (table) {
      const index = display * 3;
      rgba[j] = table[index];
      rgba[j + 1] = table[index + 1];
      rgba[j + 2] = table[index + 2];
    } else {
      rgba[j] = display;
      rgba[j + 1] = display;
      rgba[j + 2] = display;
    }
    rgba[j + 3] = 255;
  }
  return rgba;
}
