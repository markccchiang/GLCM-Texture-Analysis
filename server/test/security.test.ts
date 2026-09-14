// Security checklist of doc/ui-design-plan.md, section 8.2: server mode needs a token, the API requires it, CORS is an
// allow-list, rate limits, input limits, retention and persisted results.

import { randomBytes } from 'node:crypto';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import type { AnalysisInfo, AnalysisResults, AnalysisSettings, ImageInfo } from '@glcm/api';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { JobManager } from '../src/analysis/JobManager.js';
import { loadConfig, MIN_TOKEN_LENGTH, validateConfig } from '../src/config.js';
import { bearerToken } from '../src/security.js';
import { ImageStore } from '../src/storage/ImageStore.js';
import { ResultStore } from '../src/storage/ResultStore.js';
import { purgeExpired } from '../src/storage/retention.js';
import { createTestApp, encodeTiff, multipartBody, type TestApp } from './helpers.js';

const TOKEN = randomBytes(32).toString('base64url');
const HARALICK = encodeTiff({ width: 4, height: 4, bitsPerSample: 8, samplesPerPixel: 1, data: [0, 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 2, 2, 2, 3, 3] });
const SETTINGS: AnalysisSettings = {
  features: ['Contrast'],
  grayLevels: 4,
  quantization: { method: 'none', min: 0, max: 255, binWidth: 0 },
  distances: [1],
  directions: [0, 45, 90, 135],
  aggregation: 'perDirectionAndMean',
  logBase: 'natural',
  score: { enabled: false, age: 40, coefficients: [1.138, -1.814, 1.416, 1.714], profile: 'calibration', intensityMin: 0, intensityMax: 255 },
};
const WHOLE = { id: 'whole', name: 'Whole', shape: { type: 'rectangle' as const, x: 0, y: 0, width: 4, height: 4 } };

const auth = (token = TOKEN) => ({ authorization: `Bearer ${token}` });

async function upload(t: TestApp, headers: Record<string, string> = {}): Promise<ImageInfo> {
  const { payload, headers: multipartHeaders } = multipartBody('haralick.tif', HARALICK);
  const response = await t.app.inject({ method: 'POST', url: '/api/v1/images', payload, headers: { ...multipartHeaders, ...headers } });
  expect(response.statusCode).toBe(201);
  return response.json<ImageInfo>();
}

async function measure(t: TestApp, image: ImageInfo, headers: Record<string, string> = {}): Promise<AnalysisInfo> {
  const started = await t.app.inject({ method: 'POST', url: '/api/v1/analyses', headers, payload: { imageId: image.imageId, rois: [WHOLE], settings: SETTINGS } });
  const info = started.json<AnalysisInfo>();
  await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${info.analysisId}/events`, headers });
  return info;
}

describe('configuration', () => {
  it('uses server-mode defaults on other addresses and requires a strong token', () => {
    const server = loadConfig({ GLCM_HOST: '0.0.0.0' });
    expect(server).toMatchObject({ dataDir: '/data', maxUploadBytes: 100 * 1024 * 1024, rateLimitPerMinute: 600, retentionHours: 168, apiToken: null });
    expect(() => validateConfig(server)).toThrow(/requires GLCM_API_TOKEN/);
    expect(() => validateConfig(loadConfig({ GLCM_HOST: '0.0.0.0', GLCM_API_TOKEN: 'secret' }))).toThrow(new RegExp(`at least ${MIN_TOKEN_LENGTH}`));
    expect(() => validateConfig(loadConfig({ GLCM_HOST: '0.0.0.0', GLCM_API_TOKEN: `${TOKEN} x` }))).toThrow(/without spaces/);
    expect(() => validateConfig(loadConfig({ GLCM_HOST: '0.0.0.0', GLCM_API_TOKEN: TOKEN }))).not.toThrow();

    const local = loadConfig({});
    expect(local).toMatchObject({ host: '127.0.0.1', rateLimitPerMinute: 0, retentionHours: 0, corsOrigins: [], trustProxy: false });
    expect(() => validateConfig(local)).not.toThrow();
    expect(loadConfig({ GLCM_HOST: '0.0.0.0', GLCM_RETENTION_HOURS: '0', GLCM_RATE_LIMIT_PER_MINUTE: '0' })).toMatchObject({ retentionHours: 0, rateLimitPerMinute: 0 });
  });

  it('checks CORS origins and flags', () => {
    const config = loadConfig({ GLCM_CORS_ORIGINS: 'https://a.example.org, http://localhost:5173', GLCM_TRUST_PROXY: 'true' });
    expect(config.corsOrigins).toEqual(['https://a.example.org', 'http://localhost:5173']);
    expect(config.trustProxy).toBe(true);
    expect(() => validateConfig(config)).not.toThrow();
    expect(() => validateConfig(loadConfig({ GLCM_CORS_ORIGINS: 'https://a.example.org/path' }))).toThrow(/not an origin/);
    expect(() => validateConfig(loadConfig({ GLCM_CORS_ORIGINS: '*' }))).toThrow(/not an origin/);
    expect(() => loadConfig({ GLCM_TRUST_PROXY: 'yes' })).toThrow(/true or false/);
  });

  it('parses bearer tokens strictly', () => {
    expect(bearerToken(`Bearer ${TOKEN}`)).toBe(TOKEN);
    expect(bearerToken(`bearer ${TOKEN}`)).toBe(TOKEN);
    expect(bearerToken(`Basic ${TOKEN}`)).toBeNull();
    expect(bearerToken('Bearer a b')).toBeNull();
    expect(bearerToken(undefined)).toBeNull();
  });
});

describe('authentication', () => {
  let t: TestApp;

  beforeAll(async () => {
    t = await createTestApp({ apiToken: TOKEN });
  });

  afterAll(async () => {
    await t.close();
  });

  it('keeps /health public and reports that a token is needed', async () => {
    const response = await t.app.inject({ method: 'GET', url: '/api/v1/health' });
    expect(response.statusCode).toBe(200);
    expect(response.json()).toMatchObject({ mode: 'local', authentication: 'bearer' });
  });

  it('rejects missing and wrong tokens without details', async () => {
    const attempts = [{}, auth('wrong'), auth(`${TOKEN}x`), { authorization: `Basic ${TOKEN}` }, { authorization: TOKEN }];
    for (const headers of attempts) {
      for (const url of ['/api/v1/catalog', '/api/v1/samples', `/api/v1/images/img_${'0'.repeat(32)}`, '/api/v1/nothing-here']) {
        const response = await t.app.inject({ method: 'GET', url, headers });
        expect(response.statusCode, url).toBe(401);
        expect(response.headers['www-authenticate']).toBe('Bearer');
        expect(response.json()).toEqual({ error: 'Unauthorized', message: 'Authentication required' });
      }
    }
    const { payload, headers } = multipartBody('haralick.tif', HARALICK);
    expect((await t.app.inject({ method: 'POST', url: '/api/v1/images', payload, headers })).statusCode).toBe(401);
  });

  it('accepts the token on every route', async () => {
    expect((await t.app.inject({ method: 'GET', url: '/api/v1/catalog', headers: auth() })).statusCode).toBe(200);
    const image = await upload(t, auth());
    const raw = await t.app.inject({ method: 'GET', url: `/api/v1/images/${image.imageId}/raw`, headers: auth() });
    expect(raw.statusCode).toBe(200);
    const info = await measure(t, image, auth());
    const results = await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${info.analysisId}/results`, headers: auth() });
    expect(results.json<AnalysisResults>().results[0].status).toBe('ok');
    expect((await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${info.analysisId}/events` })).statusCode).toBe(401);
  });

  it('does not protect paths outside the API', async () => {
    const response = await t.app.inject({ method: 'GET', url: '/not-api' });
    expect(response.statusCode).toBe(404);
  });
});

describe('CORS', () => {
  it('allows only configured origins', async () => {
    const t = await createTestApp({ corsOrigins: ['https://glcm.example.org'], apiToken: TOKEN });
    try {
      const allowed = await t.app.inject({ method: 'GET', url: '/api/v1/catalog', headers: { origin: 'https://glcm.example.org', ...auth() } });
      expect(allowed.headers['access-control-allow-origin']).toBe('https://glcm.example.org');
      expect(String(allowed.headers['access-control-expose-headers'])).toContain('Content-Disposition');

      const other = await t.app.inject({ method: 'GET', url: '/api/v1/catalog', headers: { origin: 'https://evil.example.com', ...auth() } });
      expect(other.headers['access-control-allow-origin']).toBeUndefined();

      // Preflight requests carry no token
      const preflight = await t.app.inject({
        method: 'OPTIONS',
        url: '/api/v1/analyses',
        headers: { origin: 'https://glcm.example.org', 'access-control-request-method': 'POST', 'access-control-request-headers': 'authorization,content-type' },
      });
      expect(preflight.statusCode).toBe(204);
      expect(String(preflight.headers['access-control-allow-headers']).toLowerCase()).toContain('authorization');
    } finally {
      await t.close();
    }
  });

  it('sends no CORS headers by default', async () => {
    const t = await createTestApp();
    try {
      const response = await t.app.inject({ method: 'GET', url: '/api/v1/health', headers: { origin: 'https://glcm.example.org' } });
      expect(response.headers['access-control-allow-origin']).toBeUndefined();
    } finally {
      await t.close();
    }
  });
});

describe('limits', () => {
  it('limits requests per token', async () => {
    const t = await createTestApp({ rateLimitPerMinute: 3, apiToken: TOKEN });
    try {
      for (let i = 0; i < 3; i += 1) {
        expect((await t.app.inject({ method: 'GET', url: '/api/v1/catalog', headers: auth() })).statusCode).toBe(200);
      }
      const limited = await t.app.inject({ method: 'GET', url: '/api/v1/catalog', headers: auth() });
      expect(limited.statusCode).toBe(429);
      expect(limited.json()).toMatchObject({ error: 'TooManyRequests' });
      expect(limited.headers['retry-after']).toBeDefined();
    } finally {
      await t.close();
    }
  });

  it('rejects requests beyond the input limits', async () => {
    const t = await createTestApp({ maxUploadBytes: 100 });
    try {
      const rois = Array.from({ length: 1001 }, (_, i) => ({ ...WHOLE, id: `r${i}` }));
      const tooMany = await t.app.inject({ method: 'POST', url: '/api/v1/analyses', payload: { imageId: `img_${'0'.repeat(32)}`, rois, settings: SETTINGS } });
      expect(tooMany.statusCode).toBe(400);
      const polygon = { ...WHOLE, shape: { type: 'polygon', points: Array.from({ length: 10001 }, (_, i) => [i, i]) } };
      const tooManyVertices = await t.app.inject({ method: 'POST', url: '/api/v1/analyses', payload: { imageId: `img_${'0'.repeat(32)}`, rois: [polygon], settings: SETTINGS } });
      expect(tooManyVertices.statusCode).toBe(400);
      const { payload, headers } = multipartBody('big.tif', Buffer.alloc(1000));
      expect((await t.app.inject({ method: 'POST', url: '/api/v1/images', payload, headers })).statusCode).toBe(413);
    } finally {
      await t.close();
    }
  });
});

describe('storage', () => {
  let dataDir: string;

  beforeAll(async () => {
    dataDir = await fs.mkdtemp(path.join(os.tmpdir(), 'glcm-storage-test-'));
  });

  afterAll(async () => {
    await fs.rm(dataDir, { recursive: true, force: true });
  });

  it('keeps finished analyses across restarts', async () => {
    const first = await createTestApp({}, { dataDir });
    const image = await upload(first);
    const info = await measure(first, image);
    const results = (await first.app.inject({ method: 'GET', url: `/api/v1/analyses/${info.analysisId}/results` })).json<AnalysisResults>();
    await first.close();

    const second = await createTestApp({}, { dataDir });
    try {
      const status = await second.app.inject({ method: 'GET', url: `/api/v1/analyses/${info.analysisId}` });
      expect(status.json<AnalysisInfo>()).toMatchObject({ status: 'completed', completed: 1 });
      const again = await second.app.inject({ method: 'GET', url: `/api/v1/analyses/${info.analysisId}/results` });
      expect(again.json<AnalysisResults>().results).toEqual(results.results);
      const events = await second.app.inject({ method: 'GET', url: `/api/v1/analyses/${info.analysisId}/events` });
      expect(events.body).toContain('event: finished');
      expect((await second.app.inject({ method: 'DELETE', url: `/api/v1/analyses/${info.analysisId}` })).statusCode).toBe(204);
      expect((await second.app.inject({ method: 'GET', url: `/api/v1/analyses/ana_${'9'.repeat(32)}` })).statusCode).toBe(404);
    } finally {
      await second.close();
    }
  });

  it('deletes expired images and results', async () => {
    const directory = await fs.mkdtemp(path.join(dataDir, 'retention-'));
    const t = await createTestApp({}, { dataDir: directory });
    const old = await upload(t);
    const recent = await upload(t);
    const oldAnalysis = await measure(t, old);
    await t.close();

    // Age the first image and analysis by two hours
    const images = new ImageStore(directory);
    const results = new ResultStore(directory);
    const twoHoursAgo = new Date(Date.now() - 2 * 60 * 60_000).toISOString();
    const infoPath = path.join(images.imagesDir, old.imageId, 'info.json');
    await fs.writeFile(infoPath, JSON.stringify({ ...JSON.parse(await fs.readFile(infoPath, 'utf8')), createdAt: twoHoursAgo }));
    const stored = (await results.load(oldAnalysis.analysisId))!;
    await results.save({ ...stored, info: { ...stored.info, finishedAt: twoHoursAgo } });

    const restarted = await createTestApp({ retentionHours: 1 }, { dataDir: directory, retention: true });
    try {
      await expect
        .poll(async () => (await restarted.app.inject({ method: 'GET', url: `/api/v1/images/${old.imageId}` })).statusCode)
        .toBe(404);
      expect((await restarted.app.inject({ method: 'GET', url: `/api/v1/images/${recent.imageId}` })).statusCode).toBe(200);
      await expect
        .poll(async () => (await restarted.app.inject({ method: 'GET', url: `/api/v1/analyses/${oldAnalysis.analysisId}` })).statusCode)
        .toBe(404);
    } finally {
      await restarted.close();
    }
  });

  it('purges by age and keeps running analyses', async () => {
    const directory = await fs.mkdtemp(path.join(dataDir, 'purge-'));
    const images = new ImageStore(directory);
    const results = new ResultStore(directory);
    await images.init();
    await results.init();
    const jobs = new JobManager({ concurrency: 1, retainFinished: 10 });
    expect(await purgeExpired({ images, results, jobs }, 60_000)).toEqual({ images: 0, analyses: 0 });
  });
});
