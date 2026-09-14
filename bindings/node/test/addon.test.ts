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

// Haralick, Shanmugam and Dinstein (1973), figure 2: the 4 x 4 example image with gray levels 0-3
const HARALICK_WIDTH = 4;
const HARALICK_HEIGHT = 4;
const HARALICK_PIXELS = Buffer.from([0, 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 2, 2, 2, 3, 3]);
const FULL_IMAGE_ROI = { id: 'r1', name: 'Whole image', shape: { type: 'rectangle', x: 0, y: 0, width: 4, height: 4 } };

describe('roiStats', () => {
  it('counts pixels with the pixel-centre rule and reports original intensities', async () => {
    const rois = [
      { id: 'a', shape: { type: 'rectangle', x: 1, y: 1, width: 2, height: 2 } },
      { id: 'b', shape: { type: 'ellipse', cx: 2, cy: 2, rx: 0, ry: 3 } },
      { id: 'c', shape: { type: 'polygon', points: [[0, 0], [4, 0]] } },
      FULL_IMAGE_ROI,
    ];
    const stats = await native.roiStats(HARALICK_PIXELS, HARALICK_WIDTH, HARALICK_HEIGHT, 8, JSON.stringify(rois));
    expect(stats).toHaveLength(4);
    // Pixels (1, 1), (2, 1), (1, 2), (2, 2) = 0, 1, 2, 2
    expect(stats[0]).toMatchObject({ pixelCount: 4, boundingBox: { x: 1, y: 1, width: 2, height: 2 }, min: 0, max: 2, mean: 1.25, error: null });
    expect(stats[0].std).toBeCloseTo(Math.sqrt(((0 - 1.25) ** 2 + (1 - 1.25) ** 2 + 2 * (2 - 1.25) ** 2) / 3), 12);
    for (const empty of [stats[1], stats[2]]) {
      expect(empty).toEqual({ pixelCount: 0, boundingBox: null, min: null, max: null, mean: null, std: null, error: null });
    }
    expect(stats[3]).toMatchObject({ pixelCount: 16, min: 0, max: 3, mean: 1.25 });
  });

  it('reads 16-bit samples', async () => {
    const pixels = Buffer.alloc(8);
    [1000, 60000, 3, 65535].forEach((value, i) => pixels.writeUInt16LE(value, 2 * i));
    const rois = [{ shape: { type: 'rectangle', x: 0, y: 0, width: 2, height: 2 } }];
    const [stats] = await native.roiStats(pixels, 2, 2, 16, JSON.stringify(rois));
    expect(stats).toMatchObject({ pixelCount: 4, min: 3, max: 65535, mean: (1000 + 60000 + 3 + 65535) / 4 });
  });

  it('rejects malformed ROI lists', async () => {
    expect(await rejectionCode(native.roiStats(HARALICK_PIXELS, 4, 4, 8, '[{"shape": {"type": "circle"}}]'))).toBe('INVALID_ARGUMENT');
    expect(await rejectionCode(native.roiStats(HARALICK_PIXELS, 4, 4, 8, 'not json'))).toBe('INVALID_ARGUMENT');
    expect(() => native.roiStats(HARALICK_PIXELS, 4, 4, 8, 42 as unknown as string)).toThrow(TypeError);
  });
});

describe('analysis', () => {
  const settings = {
    features: ['Mean', 'Contrast'],
    grayLevels: 4,
    quantization: { method: 'none' },
    distances: [1],
    directions: [0, 45, 90, 135],
  };

  function codeOf(action: () => void): string | undefined {
    try {
      action();
    } catch (error) {
      return (error as { code?: string }).code;
    }
    return 'no error';
  }

  it('validates requests synchronously', () => {
    expect(native.validateAnalysis(JSON.stringify([FULL_IMAGE_ROI]), JSON.stringify(settings))).toBeUndefined();
    expect(codeOf(() => native.validateAnalysis('[]', JSON.stringify({ ...settings, features: ['NoSuchFeature'] })))).toBe('INVALID_ARGUMENT');
    expect(codeOf(() => native.validateAnalysis('[]', JSON.stringify({ ...settings, grayLevels: 300 })))).toBe('INVALID_ARGUMENT');
    expect(codeOf(() => native.validateAnalysis('[{"shape": {}}]', JSON.stringify(settings)))).toBe('INVALID_ARGUMENT');
    try {
      native.validateAnalysis('[]', JSON.stringify({ ...settings, distances: [0] }));
    } catch (error) {
      expect((error as Error).message).toMatch(/distance/i);
    }
  });

  it("reproduces Haralick's worked example", async () => {
    const json = await native.runAnalysis(HARALICK_PIXELS, HARALICK_WIDTH, HARALICK_HEIGHT, 8, JSON.stringify([FULL_IMAGE_ROI]), JSON.stringify(settings));
    const document = JSON.parse(json);
    expect(document).toMatchObject({ format: 'glcm-results', version: 1, coreVersion: '0.1.0' });
    expect(document.results).toHaveLength(1);
    const [result] = document.results;
    expect(result).toMatchObject({ roiId: 'r1', roiName: 'Whole image', distance: 1, status: 'ok', pixelCount: 16 });
    // Symmetric pair counts: 0° and 90° have 24 pairs, the diagonals 18
    expect(result.pairCounts).toEqual({ '0': 24, '45': 18, '90': 24, '135': 18 });
    expect(result.values.Mean.mean).toBe(1.25);
    expect(result.values.Contrast['0']).toBeCloseTo(14 / 24, 12);
    expect(result.values.Contrast['90']).toBeCloseTo(1, 12);
    expect(result.score).toBeNull();
  });

  it('runs every distance and marks unmeasurable ROIs', async () => {
    const rois = [FULL_IMAGE_ROI, { id: 'tiny', name: 'Tiny', shape: { type: 'rectangle', x: 0, y: 0, width: 1, height: 1 } }];
    const document = JSON.parse(
      await native.runAnalysis(HARALICK_PIXELS, 4, 4, 8, JSON.stringify(rois), JSON.stringify({ ...settings, distances: [1, 4] })),
    );
    expect(document.results.map((result: { roiId: string; distance: number; status: string }) => [result.roiId, result.distance, result.status])).toEqual([
      ['r1', 1, 'ok'],
      ['r1', 4, 'ok'],
      ['tiny', 1, 'skipped'],
      ['tiny', 4, 'skipped'],
    ]);
    expect(document.results[1].warnings.join(' ')).toContain('No pixel pairs at distance 4');
  });

  it('rejects invalid settings with INVALID_ARGUMENT', async () => {
    const invalid = JSON.stringify({ ...settings, grayLevels: 1 });
    expect(await rejectionCode(native.runAnalysis(HARALICK_PIXELS, 4, 4, 8, JSON.stringify([FULL_IMAGE_ROI]), invalid))).toBe('INVALID_ARGUMENT');
  });
});

describe('exports', () => {
  const settings = { features: ['Contrast'], grayLevels: 4, quantization: { method: 'none' } };

  async function haralickDocument() {
    const document = JSON.parse(
      await native.runAnalysis(HARALICK_PIXELS, HARALICK_WIDTH, HARALICK_HEIGHT, 8, JSON.stringify([FULL_IMAGE_ROI]), JSON.stringify(settings)),
    );
    return { ...document, timestamp: '2026-09-14T12:00:00Z', image: { name: 'haralick.tif', sha256: 'abc' } };
  }

  it('writes results documents again as canonical JSON and CSV', async () => {
    const document = await haralickDocument();
    const json = native.formatResults(JSON.stringify(document), 'json');
    expect(JSON.parse(json)).toEqual(document);
    expect(native.formatResults(json, 'json')).toBe(json);

    const lines = native.formatResults(json, 'csv').trimEnd().split('\n');
    expect(lines).toContain('# format=glcm-results-csv');
    expect(lines).toContain('# image=haralick.tif');
    const rows = lines.filter((line) => !line.startsWith('#')).map((line) => line.split(','));
    const header = rows[0];
    expect(rows.slice(1).map((row) => row[header.indexOf('direction')])).toEqual(['0', '45', '90', '135', 'mean']);
    expect(Number(rows[1][header.indexOf('Contrast')])).toBeCloseTo(14 / 24, 12);
  });

  it('rejects invalid documents and formats', () => {
    expect(() => native.formatResults('{"format": "glcm-results", "version": 1, "results": []}', 'csv')).toThrow(/settings is missing/);
    try {
      native.formatResults('{', 'json');
    } catch (error) {
      expect((error as { code?: string }).code).toBe('INVALID_ARGUMENT');
    }
    expect(() => native.formatResults('{}', 'xml' as 'csv')).toThrow(TypeError);
  });

  it('exports ROI crops, masks, quantized images and a manifest', async () => {
    const rois = JSON.stringify([{ id: 'a', name: 'Top/left', shape: { type: 'rectangle', x: 0, y: 0, width: 2, height: 2 } }]);
    const files = await native.exportRoiImages(HARALICK_PIXELS, 4, 4, 8, rois, '', false, false);
    expect(files.map((file) => file.name)).toEqual(['Top_left.png', 'Top_left_mask.png', 'manifest.json']);
    const crop = PNG.sync.read(files[0].data);
    expect([crop.width, crop.height]).toEqual([2, 2]);
    const manifest = JSON.parse(files[2].data.toString('utf8'));
    expect(manifest).toMatchObject({ format: 'glcm-roi-images', version: 1, entries: [{ pixelCount: 4, image: 'Top_left.png', mask: 'Top_left_mask.png' }] });

    const quantized = await native.exportRoiImages(HARALICK_PIXELS, 4, 4, 8, rois, JSON.stringify(settings), true, true);
    expect(quantized.map((file) => file.name)).toEqual(['Top_left.png', 'Top_left_mask.png', 'Top_left_q4.png', 'manifest.json']);
    expect(await rejectionCode(native.exportRoiImages(HARALICK_PIXELS, 4, 4, 8, rois, '', false, true))).toBe('INVALID_ARGUMENT');
    expect(() => native.exportRoiImages(HARALICK_PIXELS, 4, 4, 8, rois, '', 1 as unknown as boolean, false)).toThrow(TypeError);
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
