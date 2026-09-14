import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { PNG } from 'pngjs';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { windowLevel as windowLevelTs } from '@glcm/api';
import * as native from '../index.js';
import { encodeTiff } from './tiff.js';

let directory: string;

beforeAll(async () => {
  directory = await fs.mkdtemp(path.join(os.tmpdir(), 'glcm-native-test-'));
});

afterAll(async () => {
  await fs.rm(directory, { recursive: true, force: true });
});

async function writeFile(name: string, data: Buffer): Promise<string> {
  const file = path.join(directory, name);
  await fs.writeFile(file, data);
  return file;
}

function nearestRank(values: number[], perMille: number): number {
  const sorted = [...values].sort((a, b) => a - b);
  const rank = Math.max(1, Math.ceil((sorted.length * perMille) / 1000));
  return sorted[rank - 1];
}

async function rejectionCode(promise: Promise<unknown>): Promise<string | undefined> {
  try {
    await promise;
  } catch (error) {
    return (error as { code?: string }).code;
  }
  return 'resolved';
}

describe('catalog', () => {
  it('reports the core version', () => {
    expect(native.coreVersion()).toBe('0.1.0');
  });

  it('lists every feature with its flags', () => {
    const catalog = native.catalog();
    expect(catalog.features).toHaveLength(31);
    expect(new Set(catalog.features.map((feature) => feature.id)).size).toBe(31);

    const nonStandard = catalog.features.filter((feature) => feature.nonStandard).map((feature) => feature.id);
    expect(nonStandard.sort()).toEqual(['CorrelationIII', 'SumOfSquares']);
    expect(catalog.features.find((feature) => feature.id === 'MaximalCorrelationCoefficient')?.cost).toBe('slow');
    expect(catalog.features[0]).toMatchObject({ id: 'Mean', group: 'regionStatistics', docAnchor: 'equations.html#region-statistics' });

    expect(catalog.presets.map((preset) => preset.id)).toEqual(['haralick', 'clausi2002', 'basic', 'score', 'all']);
    expect(catalog.presets[0].features).toHaveLength(14);
    expect(catalog.limits).toMatchObject({
      minGrayLevels: 2,
      maxGrayLevels: 256,
      defaultGrayLevels: 32,
      maxDistance: 64,
      directions: [0, 45, 90, 135],
      quantizationMethods: ['fixedRange', 'roiMinMax', 'fixedBinWidth', 'none'],
      logBases: ['natural', 'log2'],
      defaultScoreCoefficients: { age: 1.138, mean: -1.814, entropy: 1.416, contrast: 1.714 },
    });
  });
});

describe('decodeImageFile', () => {
  it('decodes a 16-bit TIFF into little-endian samples, window and histogram', async () => {
    const width = 7;
    const height = 5;
    const values = Array.from({ length: width * height }, (_, i) => (i * 9973) % 65536);
    const file = await writeFile('sixteen.tif', encodeTiff({ width, height, bitsPerSample: 16, samplesPerPixel: 1, data: values }));

    const image = await native.decodeImageFile(file);
    expect(image).toMatchObject({ width, height, bitDepth: 16, sourceChannels: 1, warnings: [] });
    expect(image.pixels).toHaveLength(width * height * 2);
    for (let i = 0; i < values.length; i += 1) {
      expect(image.pixels.readUInt16LE(2 * i)).toBe(values[i]);
    }

    expect(image.windowMin).toBe(nearestRank(values, 5));
    expect(image.windowMax).toBe(nearestRank(values, 995));
    const histogram = new Array<number>(256).fill(0);
    for (const value of values) {
      histogram[value >> 8] += 1;
    }
    expect(image.histogram).toEqual(histogram);
  });

  it('decodes an 8-bit TIFF', async () => {
    const values = Array.from({ length: 12 }, (_, i) => i * 20);
    const file = await writeFile('eight.tif', encodeTiff({ width: 4, height: 3, bitsPerSample: 8, samplesPerPixel: 1, data: values }));

    const image = await native.decodeImageFile(file);
    expect(image).toMatchObject({ width: 4, height: 3, bitDepth: 8 });
    expect([...image.pixels]).toEqual(values);
  });

  it('converts RGB images to grayscale with a warning', async () => {
    // Pixels: red, green, blue, white
    const rgb = [255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255];
    const file = await writeFile('rgb.tif', encodeTiff({ width: 4, height: 1, bitsPerSample: 8, samplesPerPixel: 3, data: rgb }));

    const image = await native.decodeImageFile(file);
    expect(image.sourceChannels).toBe(3);
    expect(image.warnings.join(' ')).toContain('grayscale');
    // ITU-R BT.601 luma as computed by OpenCV's fixed-point conversion
    expect([...image.pixels]).toEqual([76, 150, 29, 255]);
  });

  it('rejects unsupported and unreadable files with an error code', async () => {
    const float = await writeFile(
      'float.tif',
      encodeTiff({ width: 2, height: 2, bitsPerSample: 32, samplesPerPixel: 1, sampleFormat: 'float', data: [0.5, 1, 1.5, 2] }),
    );
    expect(await rejectionCode(native.decodeImageFile(float))).toBe('UNSUPPORTED_IMAGE');

    const garbage = await writeFile('garbage.png', Buffer.from('definitely not an image'));
    expect(await rejectionCode(native.decodeImageFile(garbage))).toBe('DECODE_FAILED');
    expect(await rejectionCode(native.decodeImageFile(path.join(directory, 'missing.png')))).toBe('DECODE_FAILED');

    expect(() => native.decodeImageFile(42 as unknown as string)).toThrow(TypeError);
  });
});

describe('window/level', () => {
  it('matches the TypeScript implementation used by the browser', () => {
    const windows: Array<[number, number]> = [
      [0, 255],
      [10, 20],
      [100, 100],
      [0, 65535],
      [1000, 3000],
      [65534, 65535],
    ];
    for (const [min, max] of windows) {
      for (let value = 0; value <= 65535; value += 251) {
        expect(native.windowLevel(value, min, max)).toBe(windowLevelTs(value, min, max));
      }
      for (const value of [min - 1, min, min + 1, max - 1, max, max + 1]) {
        expect(native.windowLevel(value, min, max)).toBe(windowLevelTs(value, min, max));
      }
    }
    expect(windowLevelTs(15, 10, 20)).toBe(128);
  });
});

describe('renderDisplay', () => {
  const width = 40;
  const height = 20;
  const values = Array.from({ length: width * height }, (_, i) => (i % width) * 1000 + Math.floor(i / width) * 7);
  const pixels = Buffer.alloc(width * height * 2);
  values.forEach((value, i) => pixels.writeUInt16LE(value, 2 * i));

  it('renders the window/level mapping as a PNG', async () => {
    const png = PNG.sync.read(await native.renderDisplay(pixels, width, height, 16, 5000, 30000, 0));
    expect([png.width, png.height]).toEqual([width, height]);
    for (let i = 0; i < values.length; i += 1) {
      expect(png.data[i * 4]).toBe(windowLevelTs(values[i], 5000, 30000));
    }
  });

  it('downscales to the maximum size', async () => {
    const png = PNG.sync.read(await native.renderDisplay(pixels, width, height, 16, 0, 65535, 10));
    expect([png.width, png.height]).toEqual([10, 5]);
  });

  it('validates its arguments', async () => {
    expect(() => native.renderDisplay(pixels.subarray(1), width, height, 16, 0, 1, 0)).toThrow(TypeError);
    expect(() => native.renderDisplay(pixels, width, height, 12 as 16, 0, 1, 0)).toThrow(TypeError);
    expect(await rejectionCode(native.renderDisplay(pixels, width, height, 16, 10, 5, 0))).toBe('INVALID_ARGUMENT');
  });
});
