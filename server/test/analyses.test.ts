import type { AnalysisInfo, AnalysisRequest, AnalysisResults, AnalysisSettings, ImageInfo, RoiStatsResponse } from '@glcm/api';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { createTestApp, encodeTiff, uploadImage, type TestApp } from './helpers.js';

// Haralick, Shanmugam and Dinstein (1973), figure 2
const HARALICK = [0, 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 2, 2, 2, 3, 3];

const SETTINGS: AnalysisSettings = {
  features: ['Mean', 'Contrast'],
  grayLevels: 4,
  quantization: { method: 'none', min: 0, max: 255, binWidth: 0 },
  distances: [1],
  directions: [0, 45, 90, 135],
  aggregation: 'perDirectionAndMean',
  logBase: 'natural',
  score: { enabled: false, age: 40, coefficients: [1.138, -1.814, 1.416, 1.714], profile: 'calibration', intensityMin: 0, intensityMax: 255 },
};

const WHOLE = { id: 'whole', name: 'Whole', shape: { type: 'rectangle' as const, x: 0, y: 0, width: 4, height: 4 } };
const TOP_LEFT = { id: 'top-left', name: 'Top left', shape: { type: 'rectangle' as const, x: 0, y: 0, width: 2, height: 2 } };

interface ParsedEvent {
  event: string;
  data: unknown;
}

function parseEvents(body: string): ParsedEvent[] {
  return body
    .split('\n\n')
    .filter((block) => block.startsWith('event:'))
    .map((block) => {
      const [eventLine, dataLine] = block.split('\n');
      return { event: eventLine.slice('event: '.length), data: JSON.parse(dataLine.slice('data: '.length)) };
    });
}

async function setup(concurrency: number): Promise<{ t: TestApp; image: ImageInfo }> {
  const t = await createTestApp({ analysisConcurrency: concurrency });
  const upload = await uploadImage(t.app, 'haralick.tif', encodeTiff({ width: 4, height: 4, bitsPerSample: 8, samplesPerPixel: 1, data: HARALICK }));
  expect(upload.statusCode).toBe(201);
  return { t, image: upload.json<ImageInfo>() };
}

describe('ROI statistics', () => {
  let t: TestApp;
  let image: ImageInfo;

  beforeAll(async () => {
    ({ t, image } = await setup(2));
  });

  afterAll(async () => {
    await t.close();
  });

  it('counts pixels with the core rasterizer', async () => {
    const response = await t.app.inject({
      method: 'POST',
      url: `/api/v1/images/${image.imageId}/roi-stats`,
      payload: {
        rois: [
          { id: 'a', shape: TOP_LEFT.shape },
          { id: 'b', shape: { type: 'ellipse', cx: 2, cy: 2, rx: 1, ry: 1 } },
          { id: 'c', shape: { type: 'polygon', points: [[0, 0], [4, 0], [0, 4]] } },
        ],
      },
    });
    expect(response.statusCode).toBe(200);
    const { stats } = response.json<RoiStatsResponse>();
    expect(stats.map((s) => [s.roiId, s.pixelCount])).toEqual([
      ['a', 4],
      ['b', 4],
      ['c', 6],
    ]);
    expect(stats[0]).toMatchObject({ boundingBox: { x: 0, y: 0, width: 2, height: 2 }, min: 0, max: 0, mean: 0 });
  });

  it('validates the request', async () => {
    const bad = await t.app.inject({
      method: 'POST',
      url: `/api/v1/images/${image.imageId}/roi-stats`,
      payload: { rois: [{ id: 'a', shape: { type: 'circle', r: 3 } }] },
    });
    expect(bad.statusCode).toBe(400);
    const missing = await t.app.inject({ method: 'POST', url: `/api/v1/images/img_${'0'.repeat(32)}/roi-stats`, payload: { rois: [] } });
    expect(missing.statusCode).toBe(404);
  });
});

describe('analyses', () => {
  let t: TestApp;
  let image: ImageInfo;

  beforeAll(async () => {
    ({ t, image } = await setup(3));
  });

  afterAll(async () => {
    await t.close();
  });

  const start = (request: Partial<AnalysisRequest>) =>
    t.app.inject({ method: 'POST', url: '/api/v1/analyses', payload: { imageId: image.imageId, rois: [WHOLE], settings: SETTINGS, ...request } });

  it('streams results and reproduces the core values', async () => {
    const response = await start({ rois: [WHOLE, TOP_LEFT], settings: { ...SETTINGS, distances: [1, 2] } });
    expect(response.statusCode).toBe(202);
    const info = response.json<AnalysisInfo>();
    expect(info).toMatchObject({ imageId: image.imageId, imageName: 'haralick.tif', total: 4, coreVersion: '0.1.0' });
    expect(info.analysisId).toMatch(/^ana_[0-9a-f]{32}$/);

    // The stream ends after the "finished" event
    const stream = await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${info.analysisId}/events` });
    expect(stream.headers['content-type']).toBe('text/event-stream; charset=utf-8');
    const events = parseEvents(stream.body);
    expect(events.filter((e) => e.event === 'result')).toHaveLength(4);
    expect(events.at(-1)).toEqual({ event: 'finished', data: { status: 'completed', completed: 4, total: 4, error: null } });

    const status = await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${info.analysisId}` });
    expect(status.json<AnalysisInfo>()).toMatchObject({ status: 'completed', completed: 4 });

    const results = (await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${info.analysisId}/results` })).json<AnalysisResults>();
    expect(results).toMatchObject({ format: 'glcm-results', version: 1, status: 'completed', image: { name: 'haralick.tif', sha256: image.sha256 } });
    expect(results.results.map((r) => [r.roiId, r.distance])).toEqual([
      ['whole', 1],
      ['whole', 2],
      ['top-left', 1],
      ['top-left', 2],
    ]);
    const whole = results.results[0];
    expect(whole.status).toBe('ok');
    expect(whole.values.Contrast['0']).toBeCloseTo(14 / 24, 12);
    expect(whole.values.Contrast['90']).toBeCloseTo(1, 12);
    expect(whole.values.Mean.mean).toBe(1.25);
    // A constant 2 × 2 region has no contrast
    expect(results.results[2].values.Contrast.mean).toBe(0);
  });

  it('computes the calibration score', async () => {
    const response = await start({ settings: { ...SETTINGS, score: { ...SETTINGS.score, enabled: true } } });
    const { analysisId } = response.json<AnalysisInfo>();
    await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${analysisId}/events` });
    const [result] = (await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${analysisId}/results` })).json<AnalysisResults>().results;
    expect(result.score?.mean).toEqual(expect.any(Number));
  });

  it('rejects invalid requests before starting', async () => {
    const unknownFeature = await start({ settings: { ...SETTINGS, features: ['NoSuchFeature'] } });
    expect(unknownFeature.statusCode).toBe(400);
    expect(unknownFeature.json().message).toContain('NoSuchFeature');

    const schema = await start({ settings: { ...SETTINGS, grayLevels: 1000 } });
    expect(schema.statusCode).toBe(400);

    const noRois = await start({ rois: [] });
    expect(noRois.statusCode).toBe(400);

    const missingImage = await start({ imageId: `img_${'1'.repeat(32)}` });
    expect(missingImage.statusCode).toBe(404);

    const unknown = await t.app.inject({ method: 'GET', url: `/api/v1/analyses/ana_${'2'.repeat(32)}` });
    expect(unknown.statusCode).toBe(404);
  });

  it('reports a failed job without stopping the others', async () => {
    // Quantization "none" with Ng = 2 fails for pixel values 2 and 3
    const response = await start({ rois: [WHOLE, TOP_LEFT], settings: { ...SETTINGS, grayLevels: 2 } });
    const { analysisId } = response.json<AnalysisInfo>();
    await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${analysisId}/events` });
    const { results } = (await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${analysisId}/results` })).json<AnalysisResults>();
    expect(results.map((r) => r.status)).toEqual(['failed', 'ok']);
    expect(results[0].error).not.toBe('');
  });
});

describe('cancellation', () => {
  it('drops queued jobs', async () => {
    const { t, image } = await setup(1);
    try {
      const rois = Array.from({ length: 30 }, (_, i) => ({ ...WHOLE, id: `r${i}`, name: `ROI ${i}` }));
      const response = await t.app.inject({
        method: 'POST',
        url: '/api/v1/analyses',
        payload: { imageId: image.imageId, rois, settings: { ...SETTINGS, distances: [1, 2, 3] } },
      });
      const { analysisId, total } = response.json<AnalysisInfo>();
      expect(total).toBe(90);

      const cancel = await t.app.inject({ method: 'DELETE', url: `/api/v1/analyses/${analysisId}` });
      expect(cancel.statusCode).toBe(204);
      expect(cancel.body).toBe('');

      const events = parseEvents((await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${analysisId}/events` })).body);
      const finished = events.at(-1)!;
      expect(finished.event).toBe('finished');
      expect(finished.data).toMatchObject({ status: 'cancelled', total: 90 });
      expect((finished.data as { completed: number }).completed).toBeLessThan(90);

      const again = await t.app.inject({ method: 'DELETE', url: `/api/v1/analyses/${analysisId}` });
      expect(again.statusCode).toBe(204);
    } finally {
      await t.close();
    }
  });
});
