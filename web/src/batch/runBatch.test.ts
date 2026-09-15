import type { AnalysisInfo, AnalysisRequest, AnalysisResults, AnalysisSettings, ImageInfo, RoiSetDocument } from '@glcm/api';
import { describe, expect, it, vi } from 'vitest';
import { runBatch, type BatchDependencies, type BatchInput, type BatchItem } from './runBatch';

const settings: AnalysisSettings = {
  features: ['Contrast'],
  grayLevels: 32,
  quantization: { method: 'fixedRange', min: 0, max: 255, binWidth: 8 },
  distances: [1],
  directions: [0, 45, 90, 135],
  aggregation: 'perDirectionAndMean',
  logBase: 'natural',
  score: { enabled: false, age: 40, coefficients: [1, 1, 1, 1], profile: 'calibration', intensityMin: 0, intensityMax: 255 },
};

const catalog = { presets: [], limits: { minGrayLevels: 2, maxGrayLevels: 256, maxDistance: 64 } } as unknown as BatchInput['catalog'];

// ROI "a" lies inside a 50×50 image, "b" reaches past a 100×100 one
const roiSet: RoiSetDocument = {
  format: 'glcm-roi-set',
  version: 1,
  image: { name: 'reference.png', width: 100, height: 100, bitDepth: 8, sha256: 'reference' },
  rois: [
    { id: 'a', name: 'A', color: '#FFD400', shape: { type: 'rectangle', x: 10, y: 10, width: 20, height: 20 } },
    { id: 'b', name: 'B', shape: { type: 'rectangle', x: 80, y: 80, width: 40, height: 40 } },
  ],
};

const image = (imageId: string, width: number, height: number, bitDepth: 8 | 16) =>
  ({ imageId, name: `${imageId}.png`, width, height, bitDepth, sha256: `sha-${imageId}`, windowMin: 0, windowMax: bitDepth === 8 ? 255 : 4095 }) as ImageInfo;

const file = (name: string) => new File(['pixels'], name);

/** Uploads return the images by file name (others fail to decode); every analysis completes with one result per ROI */
function fakeServer(uploads: Record<string, ImageInfo>, stored: Record<string, ImageInfo> = {}) {
  const requests: AnalysisRequest[] = [];
  const deps = {
    sha256: vi.fn(async (source: File) => `sha-${source.name}`),
    findImagesBySha256: vi.fn(async (sha256: string) => (stored[sha256] ? [stored[sha256]] : [])),
    uploadImage: vi.fn(async (source: File, onProgress: (loaded: number, total: number) => void) => {
      onProgress(5, 10);
      const info = uploads[source.name];
      if (!info) {
        throw new Error(`The file ${source.name} could not be decoded as an image`);
      }
      return info;
    }),
    startAnalysis: vi.fn(async (request: AnalysisRequest) => {
      requests.push(request);
      return { analysisId: `ana-${requests.length}`, total: request.rois.length * request.settings.distances.length } as AnalysisInfo;
    }),
    waitForResults: vi.fn(async (analysisId: string, onProgress: (completed: number) => void) => {
      const request = requests[Number(analysisId.split('-')[1]) - 1];
      onProgress(1);
      return { analysisId, status: 'completed', results: request.rois.map((roi) => ({ roiId: roi.id, status: 'ok' })) } as unknown as AnalysisResults;
    }),
    cancelAnalysis: vi.fn(async (_analysisId: string) => undefined),
  } satisfies BatchDependencies;
  return { deps, requests };
}

const run = (files: File[], deps: BatchDependencies, options: { settings?: AnalysisSettings; signal?: AbortSignal; updates?: BatchItem[][] } = {}) =>
  runBatch({ files, roiSet, settings: options.settings ?? settings, catalog }, deps, (items) => options.updates?.push(items), options.signal ?? new AbortController().signal);

describe('runBatch', () => {
  it('uploads new images, reuses stored ones, and fits the ROIs and settings to each image', async () => {
    const { deps, requests } = fakeServer({ 'one.png': image('one', 100, 100, 8) }, { 'sha-two.tif': image('two', 50, 50, 16) });
    const updates: BatchItem[][] = [];
    const items = await run([file('one.png'), file('two.tif')], deps, { updates });

    expect(items.map((item) => item.status)).toEqual(['done', 'done']);
    expect(items.map((item) => item.reused)).toEqual([false, true]);
    expect(deps.uploadImage).toHaveBeenCalledTimes(1);

    // Clipped to the 100×100 image; a missing colour is not sent
    expect(requests[0].rois.map((roi) => roi.id)).toEqual(['a', 'b']);
    expect(requests[0].rois[1]).not.toHaveProperty('color');
    expect(items[0].message).toMatch(/clipped/);
    // Only "a" lies on the 50×50 image, and the settings follow its bit depth
    expect(requests[1].rois.map((roi) => roi.id)).toEqual(['a']);
    expect(requests[1].settings.quantization.max).toBeGreaterThan(255);
    expect(items[1].message).toMatch(/Skipped 1 ROI/);
    expect(items.map((item) => item.message ?? '').join(' ')).not.toMatch(/different image file/);

    expect(updates.some((state) => state[0].status === 'uploading' && state[0].uploaded === 0.5)).toBe(true);
    expect(updates.some((state) => state[0].status === 'measuring' && state[0].completed === 1)).toBe(true);
  });

  it('skips images without ROIs and records failures without stopping', async () => {
    const { deps } = fakeServer({ 'tiny.png': image('tiny', 5, 5, 8), 'ok.png': image('ok', 100, 100, 8) });
    const items = await run([file('tiny.png'), file('broken.png'), file('ok.png')], deps);
    expect(items.map((item) => item.status)).toEqual(['skipped', 'failed', 'done']);
    expect(items[0].message).toMatch(/Skipped 2 ROIs/);
    expect(items[1].message).toMatch(/could not be decoded/);
  });

  it('fails an image whose settings are invalid without starting an analysis', async () => {
    const { deps } = fakeServer({ 'one.png': image('one', 100, 100, 8) });
    const items = await run([file('one.png')], deps, { settings: { ...settings, grayLevels: 1 } });
    expect(items[0]).toMatchObject({ status: 'failed', message: expect.stringMatching(/Gray levels/) });
    expect(deps.startAnalysis).not.toHaveBeenCalled();
  });

  it('cancels the running analysis and the images after it', async () => {
    const { deps } = fakeServer({ 'one.png': image('one', 100, 100, 8), 'two.png': image('two', 100, 100, 8) });
    const controller = new AbortController();
    deps.waitForResults.mockImplementationOnce(async (analysisId: string) => {
      controller.abort();
      return { analysisId, status: 'cancelled', results: [] } as unknown as AnalysisResults;
    });
    const items = await run([file('one.png'), file('two.png')], deps, { signal: controller.signal });
    expect(deps.cancelAnalysis).toHaveBeenCalledWith('ana-1');
    expect(items.map((item) => item.status)).toEqual(['cancelled', 'cancelled']);
    expect(deps.startAnalysis).toHaveBeenCalledTimes(1);
  });
});
