import type { AnalysisSettings, FeatureMapInfo, FeatureMapSettings, ImageInfo } from '@glcm/api';
import * as native from '@glcm/native';
import { afterEach, describe, expect, it } from 'vitest';
import { FeatureMapManager, type FeatureMapState } from '../src/analysis/FeatureMapManager.js';
import { JobLimitError } from '../src/analysis/JobManager.js';
import { Scheduler } from '../src/analysis/Scheduler.js';
import { createTestApp, encodeTiff, uploadImage, type TestApp } from './helpers.js';

const WIDTH = 16;
const HEIGHT = 14;
const PIXELS = Array.from({ length: WIDTH * HEIGHT }, (_, i) => (i * 37 + Math.floor(i / WIDTH) * 11) % 256);

const SETTINGS: FeatureMapSettings = {
  feature: 'Contrast',
  window: 5,
  step: 4,
  grayLevels: 16,
  quantization: { method: 'fixedRange', min: 0, max: 255, binWidth: 1 },
  distance: 1,
  directions: [0, 45, 90, 135],
  logBase: 'natural',
};

let t: TestApp | undefined;

afterEach(async () => {
  await t?.close();
  t = undefined;
});

async function setup(overrides: Parameters<typeof createTestApp>[0] = {}): Promise<{ t: TestApp; image: ImageInfo }> {
  t = await createTestApp(overrides);
  const upload = await uploadImage(t.app, 'pattern.tif', encodeTiff({ width: WIDTH, height: HEIGHT, bitsPerSample: 8, samplesPerPixel: 1, data: PIXELS }));
  expect(upload.statusCode).toBe(201);
  return { t, image: upload.json<ImageInfo>() };
}

const start = (app: TestApp['app'], imageId: string, settings: FeatureMapSettings = SETTINGS) =>
  app.inject({ method: 'POST', url: '/api/v1/feature-maps', payload: { imageId, settings } });

async function waitUntilFinished(app: TestApp['app'], featureMapId: string): Promise<FeatureMapInfo> {
  for (let attempt = 0; attempt < 500; attempt += 1) {
    const info = (await app.inject({ method: 'GET', url: `/api/v1/feature-maps/${featureMapId}` })).json<FeatureMapInfo>();
    if (['completed', 'cancelled', 'failed'].includes(info.status)) {
      return info;
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw new Error('The feature map did not finish');
}

describe('feature maps', () => {
  it('computes a map in bands with the values of the addon', async () => {
    const { t, image } = await setup({ analysisConcurrency: 3 });
    const response = await start(t.app, image.imageId);
    expect(response.statusCode).toBe(202);
    const started = response.json<FeatureMapInfo>();
    expect(started).toMatchObject({ imageId: image.imageId, imageName: 'pattern.tif', step: 4, columns: 4, rows: 4, error: null, settings: SETTINGS });

    const info = await waitUntilFinished(t.app, started.featureMapId);
    expect(info).toMatchObject({ status: 'completed', completedRows: 4 });
    const values = await t.app.inject({ method: 'GET', url: `/api/v1/feature-maps/${started.featureMapId}/values` });
    expect(values.statusCode).toBe(200);
    expect(values.headers['content-type']).toBe('application/octet-stream');
    const body = values.rawPayload;
    const actual = new Float32Array(body.buffer.slice(body.byteOffset, body.byteOffset + body.byteLength));
    const pixels = Buffer.from(PIXELS);
    const expected = await native.computeFeatureMap(pixels, WIDTH, HEIGHT, 8, JSON.stringify(SETTINGS), 0, 4);
    expect(Array.from(actual)).toEqual(Array.from(expected));
    expect(actual.every(Number.isFinite)).toBe(true);
  });

  it('chooses the step automatically', async () => {
    const { t, image } = await setup();
    const info = (await start(t.app, image.imageId, { ...SETTINGS, step: null })).json<FeatureMapInfo>();
    expect(info).toMatchObject({ step: 1, columns: WIDTH, rows: HEIGHT });
    expect((await waitUntilFinished(t.app, info.featureMapId)).status).toBe('completed');
  });

  it('validates requests', async () => {
    const { t, image } = await setup();
    const invalid = [{ feature: 'MaximalCorrelationCoefficient' }, { feature: 'Mean' }, { window: 6 }, { distance: 5 }];
    for (const change of invalid) {
      const response = await start(t.app, image.imageId, { ...SETTINGS, ...change });
      expect(response.statusCode, JSON.stringify(change)).toBe(400);
    }
    expect((await start(t.app, 'img_00000000000000000000000000000000')).statusCode).toBe(404);
    const unknown = 'fmap_00000000000000000000000000000000';
    expect((await t.app.inject({ method: 'GET', url: `/api/v1/feature-maps/${unknown}` })).statusCode).toBe(404);
    expect((await t.app.inject({ method: 'DELETE', url: `/api/v1/feature-maps/${unknown}` })).statusCode).toBe(404);
  });

  it('fails when the image cannot be quantized with the settings', async () => {
    const { t, image } = await setup();
    const info = (await start(t.app, image.imageId, { ...SETTINGS, quantization: { ...SETTINGS.quantization, method: 'none' } })).json<FeatureMapInfo>();
    const finished = await waitUntilFinished(t.app, info.featureMapId);
    expect(finished.status).toBe('failed');
    expect(finished.error).toMatch(/is not below Ng = 16/);
    expect((await t.app.inject({ method: 'GET', url: `/api/v1/feature-maps/${info.featureMapId}/values` })).statusCode).toBe(409);
  });

  it('cancels queued maps and forgets finished ones', async () => {
    // No workers: the bands stay queued
    const { t, image } = await setup({ analysisConcurrency: 0 });
    const info = (await start(t.app, image.imageId)).json<FeatureMapInfo>();
    expect(info.status).toBe('queued');
    const url = `/api/v1/feature-maps/${info.featureMapId}`;
    expect((await t.app.inject({ method: 'GET', url: `${url}/values` })).json()).toMatchObject({ error: 'NotReady' });

    expect((await t.app.inject({ method: 'DELETE', url })).statusCode).toBe(204);
    expect((await t.app.inject({ method: 'GET', url })).json<FeatureMapInfo>()).toMatchObject({ status: 'cancelled', completedRows: 0 });
    expect((await t.app.inject({ method: 'DELETE', url })).statusCode).toBe(204);
    expect((await t.app.inject({ method: 'GET', url })).statusCode).toBe(404);
  });

  it('shares the pending task limit with analyses', async () => {
    // The map is one band; the analysis's two jobs do not fit next to it
    const { t, image } = await setup({ analysisConcurrency: 0, maxPendingJobs: 2 });
    expect((await start(t.app, image.imageId)).statusCode).toBe(202);

    const settings: AnalysisSettings = {
      features: ['Contrast'],
      grayLevels: 16,
      quantization: SETTINGS.quantization,
      distances: [1, 2],
      directions: [0, 45, 90, 135],
      aggregation: 'meanOnly',
      logBase: 'natural',
      score: { enabled: false, age: 40, coefficients: [0, 0, 0, 0], profile: 'calibration', intensityMin: 0, intensityMax: 255 },
    };
    const roi = { id: 'r', name: 'R', shape: { type: 'rectangle' as const, x: 0, y: 0, width: 4, height: 4 } };
    const analysis = await t.app.inject({ method: 'POST', url: '/api/v1/analyses', payload: { imageId: image.imageId, rois: [roi], settings } });
    expect(analysis.statusCode).toBe(503);
  });

});

describe('feature map manager', () => {
  const image = { imageId: `img_${'0'.repeat(32)}`, name: 'pattern.tif', width: WIDTH, height: HEIGHT, bitDepth: 8 } as ImageInfo;
  const pixels = Buffer.from(PIXELS);
  const options = { maxPendingJobs: 100, maxBands: 100, retainFinished: 8 };

  async function finished(state: FeatureMapState, timeoutMs = 10_000): Promise<void> {
    const deadline = Date.now() + timeoutMs;
    while (state.info.status === 'queued' || state.info.status === 'running') {
      if (Date.now() > deadline) {
        throw new Error('The feature map did not finish');
      }
      await new Promise((resolve) => setTimeout(resolve, 5));
    }
  }

  it('splits the rows into bands of about equal estimated work', async () => {
    const { workPerRow } = native.featureMapGrid(JSON.stringify(SETTINGS), WIDTH, HEIGHT);
    const queued = new Scheduler(0);
    new FeatureMapManager({ ...options, scheduler: queued, bandWork: 1 }).start(SETTINGS, image, pixels);
    // A band has at least one row
    expect(queued.pending).toBe(4);

    const scheduler = new Scheduler(2);
    const manager = new FeatureMapManager({ ...options, scheduler, bandWork: 2.5 * workPerRow });
    const state = manager.start(SETTINGS, image, pixels);
    expect(scheduler.pending).toBe(2);
    await finished(state);
    expect(state.info).toMatchObject({ status: 'completed', completedRows: 4 });
    const expected = await native.computeFeatureMap(pixels, WIDTH, HEIGHT, 8, JSON.stringify(SETTINGS), 0, 4);
    expect(Array.from(state.values)).toEqual(Array.from(expected));
  });

  it('refuses maps with more bands than the limit', () => {
    const scheduler = new Scheduler(0);
    const tooMany = (limits: { maxBands: number; maxPendingJobs: number }) => {
      try {
        new FeatureMapManager({ ...options, ...limits, scheduler, bandWork: 1 }).start(SETTINGS, image, pixels);
      } catch (error) {
        return error;
      }
      return undefined;
    };
    expect(tooMany({ maxBands: 3, maxPendingJobs: 100 })).toBeInstanceOf(JobLimitError);
    expect(tooMany({ maxBands: 3, maxPendingJobs: 100 })).toMatchObject({ reason: 'tooLarge', jobs: 4, limit: 3 });
    expect(tooMany({ maxBands: 100, maxPendingJobs: 2 })).toMatchObject({ reason: 'tooLarge', jobs: 4, limit: 2 });
    expect(scheduler.pending).toBe(0);
  });

  it('stops a running band when the map is cancelled', async () => {
    const size = 400;
    const large = { ...image, width: size, height: size };
    const largePixels = Buffer.from(Array.from({ length: size * size }, (_, i) => (i * 7919 + (i >> 3)) % 256));
    // Several minutes of computing in a single band
    const slow: FeatureMapSettings = { ...SETTINGS, window: 127, step: 1, grayLevels: 256, quantization: { method: 'fixedRange', min: 0, max: 255, binWidth: 1 } };
    const scheduler = new Scheduler(1);
    const manager = new FeatureMapManager({ ...options, scheduler, bandWork: Number.POSITIVE_INFINITY });
    const state = manager.start(slow, large, largePixels);
    expect(state.info.status).toBe('running');
    await new Promise((resolve) => setTimeout(resolve, 100));

    const cancelledAt = Date.now();
    expect(manager.cancel(state.info.featureMapId)).toBe(true);
    await finished(state);
    expect(Date.now() - cancelledAt).toBeLessThan(2000);
    expect(state.info).toMatchObject({ status: 'cancelled', completedRows: 0, error: null });
    expect(scheduler.pending).toBe(0);

    // The worker is free for the next map
    const next = manager.start(SETTINGS, image, pixels);
    await finished(next);
    expect(next.info.status).toBe('completed');
  });
});
