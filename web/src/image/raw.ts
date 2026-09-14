// Raw grayscale samples from GET /api/v1/images/{id}/raw (doc/ui-design-plan.md, section 6.1).

import { RAW_HEADERS } from '@glcm/api';

export interface RawImage {
  width: number;
  height: number;
  bitDepth: 8 | 16;
  /** Row-major samples in native byte order */
  samples: Uint8Array | Uint16Array;
}

export interface RawFormat {
  width: number;
  height: number;
  bitDepth: 8 | 16;
  byteOrder: string;
}

export function hostIsLittleEndian(): boolean {
  return new Uint8Array(new Uint16Array([1]).buffer)[0] === 1;
}

function positiveInteger(text: string | null, name: string): number {
  const value = Number(text);
  if (text === null || !Number.isSafeInteger(value) || value <= 0) {
    throw new Error(`Invalid ${name} header: ${text}`);
  }
  return value;
}

/** Size and format from the X-Image-* response headers */
export function rawFormatFromHeaders(headers: Headers): RawFormat {
  const bitDepth = positiveInteger(headers.get(RAW_HEADERS.bitDepth), RAW_HEADERS.bitDepth);
  if (bitDepth !== 8 && bitDepth !== 16) {
    throw new Error(`Unsupported bit depth: ${bitDepth}`);
  }
  return {
    width: positiveInteger(headers.get(RAW_HEADERS.width), RAW_HEADERS.width),
    height: positiveInteger(headers.get(RAW_HEADERS.height), RAW_HEADERS.height),
    bitDepth,
    byteOrder: headers.get(RAW_HEADERS.byteOrder) ?? '',
  };
}

/**
 * Wraps or copies the response body into typed samples, checking that its length matches the headers. 16-bit samples
 * are viewed in place on little-endian machines and copied with DataView otherwise.
 */
export function decodeRawSamples(buffer: ArrayBuffer, format: RawFormat, littleEndianHost = hostIsLittleEndian()): RawImage {
  if (format.byteOrder !== 'little-endian') {
    throw new Error(`Unsupported byte order: ${format.byteOrder || '(missing)'}`);
  }
  const bytesPerSample = format.bitDepth / 8;
  const expected = format.width * format.height * bytesPerSample;
  if (buffer.byteLength !== expected) {
    throw new Error(`Raw data has ${buffer.byteLength} bytes, expected ${expected} for ${format.width}×${format.height} ${format.bitDepth}-bit`);
  }

  const { width, height, bitDepth } = format;
  if (bitDepth === 8) {
    return { width, height, bitDepth, samples: new Uint8Array(buffer) };
  }
  if (littleEndianHost) {
    return { width, height, bitDepth, samples: new Uint16Array(buffer) };
  }
  const view = new DataView(buffer);
  const samples = new Uint16Array(width * height);
  for (let i = 0; i < samples.length; i += 1) {
    samples[i] = view.getUint16(2 * i, true);
  }
  return { width, height, bitDepth, samples };
}

export function sampleAt(image: RawImage, x: number, y: number): number {
  return image.samples[y * image.width + x];
}
