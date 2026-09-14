import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import type { SamplesResponse } from '@glcm/api';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { createTestApp, type TestApp } from './helpers.js';

const INDEX_HTML = '<!doctype html><title>GLCM</title><div id="root"></div>';

describe('web app and samples', () => {
  let t: TestApp;
  let fixtures: string;

  beforeAll(async () => {
    fixtures = await fs.mkdtemp(path.join(os.tmpdir(), 'glcm-web-test-'));
    const webDir = path.join(fixtures, 'dist');
    const samplesDir = path.join(fixtures, 'samples');
    await fs.mkdir(path.join(webDir, 'assets'), { recursive: true });
    await fs.writeFile(path.join(webDir, 'index.html'), INDEX_HTML);
    await fs.writeFile(path.join(webDir, 'assets', 'index-abc123.js'), 'console.log(1);');

    await fs.mkdir(path.join(samplesDir, 'textures'), { recursive: true });
    await fs.writeFile(path.join(samplesDir, 'lena.jpg'), Buffer.from([0xff, 0xd8, 0xff]));
    await fs.writeFile(path.join(samplesDir, 'textures', 'brick.png'), Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d]));
    await fs.writeFile(path.join(samplesDir, 'README.md'), '# not an image');
    await fs.writeFile(path.join(samplesDir, '.hidden.png'), 'hidden');
    await fs.writeFile(path.join(fixtures, 'secret.png'), 'outside');
    await fs.symlink(path.join(fixtures, 'secret.png'), path.join(samplesDir, 'link.png'));

    t = await createTestApp({ webDir, samplesDir });
  });

  afterAll(async () => {
    await t.close();
    await fs.rm(fixtures, { recursive: true, force: true });
  });

  it('lists image files only', async () => {
    const response = await t.app.inject({ method: 'GET', url: '/api/v1/samples' });
    expect(response.statusCode).toBe(200);
    expect(response.json<SamplesResponse>()).toEqual({
      samples: [
        { path: 'lena.jpg', name: 'lena.jpg', group: '', sizeBytes: 3 },
        { path: 'textures/brick.png', name: 'brick.png', group: 'textures', sizeBytes: 5 },
      ],
      defaultSample: 'lena.jpg',
    });
  });

  it('downloads listed samples and nothing else', async () => {
    const brick = await t.app.inject({ method: 'GET', url: '/api/v1/samples/file', query: { path: 'textures/brick.png' } });
    expect(brick.statusCode).toBe(200);
    expect(brick.headers['content-type']).toBe('image/png');
    expect(brick.rawPayload).toEqual(Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d]));

    for (const samplePath of ['README.md', 'link.png', '.hidden.png', '../secret.png', 'textures/../../secret.png', path.join(fixtures, 'secret.png')]) {
      const response = await t.app.inject({ method: 'GET', url: '/api/v1/samples/file', query: { path: samplePath } });
      expect(response.statusCode, samplePath).toBe(404);
      expect(response.json(), samplePath).toMatchObject({ error: 'NotFound' });
    }
  });

  it('serves the web app with cache headers', async () => {
    const index = await t.app.inject({ method: 'GET', url: '/', headers: { accept: 'text/html' } });
    expect(index.statusCode).toBe(200);
    expect(index.body).toBe(INDEX_HTML);
    expect(index.headers['cache-control']).toBe('no-cache');

    const asset = await t.app.inject({ method: 'GET', url: '/assets/index-abc123.js' });
    expect(asset.statusCode).toBe(200);
    expect(asset.headers['cache-control']).toBe('public, max-age=31536000, immutable');
  });

  it('serves files created after startup, e.g. a rebuilt app', async () => {
    await fs.writeFile(path.join(fixtures, 'dist', 'assets', 'index-rebuilt.js'), 'console.log(2);');
    const asset = await t.app.inject({ method: 'GET', url: '/assets/index-rebuilt.js' });
    expect(asset.statusCode).toBe(200);
    expect(asset.body).toBe('console.log(2);');
    expect(asset.headers['cache-control']).toBe('public, max-age=31536000, immutable');
  });

  it('falls back to index.html for page requests but keeps JSON 404s for the API', async () => {
    const page = await t.app.inject({ method: 'GET', url: '/some/client/route', headers: { accept: 'text/html,*/*' } });
    expect(page.statusCode).toBe(200);
    expect(page.body).toBe(INDEX_HTML);

    const api = await t.app.inject({ method: 'GET', url: '/api/v1/unknown', headers: { accept: 'text/html' } });
    expect(api.statusCode).toBe(404);
    expect(api.json()).toMatchObject({ error: 'NotFound' });

    const script = await t.app.inject({ method: 'GET', url: '/assets/missing.js', headers: { accept: '*/*' } });
    expect(script.statusCode).toBe(404);
  });
});

describe('without a web build or samples', () => {
  it('returns JSON 404s and an empty sample list', async () => {
    const t = await createTestApp();
    try {
      const page = await t.app.inject({ method: 'GET', url: '/', headers: { accept: 'text/html' } });
      expect(page.statusCode).toBe(404);
      expect(page.json()).toMatchObject({ error: 'NotFound' });
      const samples = await t.app.inject({ method: 'GET', url: '/api/v1/samples' });
      expect(samples.json()).toEqual({ samples: [], defaultSample: null });
    } finally {
      await t.close();
    }
  });
});
