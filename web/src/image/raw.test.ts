import { describe, expect, it } from 'vitest';
import { decodeRawSamples, hostIsLittleEndian, rawFormatFromHeaders, sampleAt, type RawFormat } from './raw';

function littleEndian16(values: number[]): ArrayBuffer {
  const buffer = new ArrayBuffer(values.length * 2);
  const view = new DataView(buffer);
  values.forEach((value, i) => view.setUint16(2 * i, value, true));
  return buffer;
}

const format16: RawFormat = { width: 3, height: 2, bitDepth: 16, byteOrder: 'little-endian' };
const values16 = [0, 1, 255, 256, 40000, 65535];

describe('rawFormatFromHeaders', () => {
  it('reads the X-Image-* headers', () => {
    const headers = new Headers({
      'x-image-width': '640',
      'x-image-height': '480',
      'x-image-bit-depth': '16',
      'x-image-byte-order': 'little-endian',
    });
    expect(rawFormatFromHeaders(headers)).toEqual({ width: 640, height: 480, bitDepth: 16, byteOrder: 'little-endian' });
  });

  it('rejects missing or invalid headers', () => {
    expect(() => rawFormatFromHeaders(new Headers({ 'x-image-height': '1', 'x-image-bit-depth': '8' }))).toThrow(/x-image-width/);
    expect(() =>
      rawFormatFromHeaders(new Headers({ 'x-image-width': '2', 'x-image-height': '1', 'x-image-bit-depth': '12' })),
    ).toThrow(/bit depth/);
  });
});

describe('decodeRawSamples', () => {
  it('views 16-bit samples in place on little-endian hosts', () => {
    const image = decodeRawSamples(littleEndian16(values16), format16, true);
    expect(Array.from(image.samples)).toEqual(values16);
    expect(sampleAt(image, 1, 1)).toBe(40000);
  });

  it('copies 16-bit samples with DataView on big-endian hosts', () => {
    const image = decodeRawSamples(littleEndian16(values16), format16, false);
    expect(image.samples).toBeInstanceOf(Uint16Array);
    expect(Array.from(image.samples)).toEqual(values16);
  });

  it('matches the host check', () => {
    const image = decodeRawSamples(littleEndian16(values16), format16, hostIsLittleEndian());
    expect(Array.from(image.samples)).toEqual(values16);
  });

  it('wraps 8-bit samples', () => {
    const image = decodeRawSamples(new Uint8Array([1, 2, 3, 4]).buffer, { width: 2, height: 2, bitDepth: 8, byteOrder: 'little-endian' });
    expect(image.samples).toBeInstanceOf(Uint8Array);
    expect(sampleAt(image, 0, 1)).toBe(3);
  });

  it('checks the length and byte order', () => {
    expect(() => decodeRawSamples(new ArrayBuffer(11), format16)).toThrow(/expected 12/);
    expect(() => decodeRawSamples(littleEndian16(values16), { ...format16, byteOrder: 'big-endian' })).toThrow(/byte order/);
  });
});
