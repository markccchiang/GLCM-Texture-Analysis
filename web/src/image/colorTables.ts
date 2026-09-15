// Colour tables of the display: the 8-bit display value from the window/level mapping selects a colour. They change
// only what is shown; measurements always use the stored intensities.

import { COLOR_TABLE_DATA } from './colorTableData';

export type ColorTableId = 'gray' | 'inverted' | 'viridis' | 'magma' | 'hot';

export interface ColorTable {
  id: ColorTableId;
  name: string;
  /** RGB bytes for the display values 0-255 (768 bytes) */
  rgb: Uint8Array;
}

function ramp(inverted: boolean): Uint8Array {
  const rgb = new Uint8Array(768);
  for (let value = 0; value < 256; value += 1) {
    rgb.fill(inverted ? 255 - value : value, value * 3, value * 3 + 3);
  }
  return rgb;
}

function decode(base64: string): Uint8Array {
  const binary = atob(base64);
  return Uint8Array.from(binary, (character) => character.charCodeAt(0));
}

export const COLOR_TABLES: readonly ColorTable[] = [
  { id: 'gray', name: 'Gray', rgb: ramp(false) },
  { id: 'inverted', name: 'Inverted', rgb: ramp(true) },
  { id: 'viridis', name: 'Viridis', rgb: decode(COLOR_TABLE_DATA.viridis) },
  { id: 'magma', name: 'Magma', rgb: decode(COLOR_TABLE_DATA.magma) },
  { id: 'hot', name: 'Hot', rgb: decode(COLOR_TABLE_DATA.hot) },
];

export function colorTableById(id: ColorTableId): ColorTable {
  return COLOR_TABLES.find((table) => table.id === id) ?? COLOR_TABLES[0];
}

export function colorAt(table: ColorTable, value: number): [number, number, number] {
  const index = Math.min(255, Math.max(0, Math.round(value))) * 3;
  return [table.rgb[index], table.rgb[index + 1], table.rgb[index + 2]];
}

/** Evenly spaced colours of the table, from display value 0 to 255 */
export function colorStops(table: ColorTable, count = 16): Array<{ offset: number; color: string }> {
  return Array.from({ length: count }, (_, k) => {
    const [r, g, b] = colorAt(table, (k * 255) / (count - 1));
    return { offset: k / (count - 1), color: `rgb(${r}, ${g}, ${b})` };
  });
}

/** A CSS linear-gradient of the table, for swatches */
export function cssGradient(table: ColorTable, count = 16): string {
  return `linear-gradient(to right, ${colorStops(table, count)
    .map(({ offset, color }) => `${color} ${Math.round(offset * 100)}%`)
    .join(', ')})`;
}

/** Colours opaque gray RGBA pixels (R = G = B, e.g. a decoded display.png) with the table, in place */
export function colorizeRgba(rgba: Uint8ClampedArray, table: ColorTable): void {
  if (table.id === 'gray') {
    return;
  }
  const rgb = table.rgb;
  for (let i = 0; i < rgba.length; i += 4) {
    const index = rgba[i] * 3;
    rgba[i] = rgb[index];
    rgba[i + 1] = rgb[index + 1];
    rgba[i + 2] = rgb[index + 2];
  }
}
