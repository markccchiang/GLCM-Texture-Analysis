import type { GradientStatsResponse, ImageInfo, LivewireResponse } from '@glcm/api';
import * as native from '@glcm/native';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { createTestApp, encodeTiff, uploadImage, type TestApp } from './helpers.js';

// Dark background with a bright square [10, 30) x [10, 30)
const SIZE = 40;
const DATA = Array.from({ length: SIZE * SIZE }, (_, i) => {
  const [x, y] = [i % SIZE, Math.floor(i / SIZE)];
  return x >= 10 && x < 30 && y >= 10 && y < 30 ? 220 : 20;
});

describe('edge maps and livewire', () => {
  let t: TestApp;
  let image: ImageInfo;
  const pixels = Buffer.from(DATA);

  beforeAll(async () => {
    t = await createTestApp();
    const upload = await uploadImage(t.app, 'square.tif', encodeTiff({ width: SIZE, height: SIZE, bitsPerSample: 8, samplesPerPixel: 1, data: DATA }));
    expect(upload.statusCode).toBe(201);
    image = upload.json<ImageInfo>();
  });

  afterAll(async () => {
    await t.close();
  });

  const get = (path: string, headers: Record<string, string> = {}) => t.app.inject({ method: 'GET', url: `/api/v1/images/${image.imageId}/${path}`, headers });

  it('renders edge maps with the core, with ETags', async () => {
    const response = await get('edges.png?method=canny&sigma=1&low=20&high=60');
    expect(response.statusCode).toBe(200);
    expect(response.headers['content-type']).toBe('image/png');
    const expected = await native.renderEdgeMap(pixels, SIZE, SIZE, 8, 'canny', 1, 20, 60, t.config.displayMaxSize);
    expect(response.rawPayload.equals(expected)).toBe(true);

    const again = await get('edges.png?method=canny&sigma=1&low=20&high=60', { 'if-none-match': String(response.headers.etag) });
    expect(again.statusCode).toBe(304);

    const sobel = await get('edges.png?method=sobel&sigma=0&low=0&high=100&maxSize=20');
    expect(sobel.rawPayload.equals(await native.renderEdgeMap(pixels, SIZE, SIZE, 8, 'sobel', 0, 0, 100, 20))).toBe(true);
  });

  it('gives gradient statistics', async () => {
    const response = await get('gradient-stats?sigma=1.5');
    expect(response.statusCode).toBe(200);
    expect(response.json<GradientStatsResponse>()).toEqual(await native.gradientStatistics(pixels, SIZE, SIZE, 8, 1.5));
  });

  it('finds livewire paths', async () => {
    const response = await t.app.inject({
      method: 'POST',
      url: `/api/v1/images/${image.imageId}/livewire`,
      payload: { from: { x: 10, y: 20 }, to: { x: 29, y: 20 }, sigma: 0 },
    });
    expect(response.statusCode).toBe(200);
    expect(response.json<LivewireResponse>().points).toEqual(await native.livewirePath(pixels, SIZE, SIZE, 8, 10, 20, 29, 20, 0));
  });

  it('validates requests', async () => {
    expect((await get('edges.png?method=sobel&sigma=1&low=50&high=50')).statusCode).toBe(400);
    expect((await get('edges.png?sigma=1&low=0&high=50')).statusCode).toBe(400);
    expect((await get('edges.png?method=canny&sigma=11&low=0&high=50')).statusCode).toBe(400);
    expect((await get('gradient-stats')).statusCode).toBe(400);
    const outside = await t.app.inject({
      method: 'POST',
      url: `/api/v1/images/${image.imageId}/livewire`,
      payload: { from: { x: 0, y: 0 }, to: { x: SIZE, y: 0 }, sigma: 1 },
    });
    expect(outside.statusCode).toBe(400);
    expect((await t.app.inject({ method: 'GET', url: `/api/v1/images/img_${'0'.repeat(32)}/gradient-stats?sigma=1` })).statusCode).toBe(404);
  });
});
