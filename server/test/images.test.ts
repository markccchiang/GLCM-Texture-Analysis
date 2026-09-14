import { createHash } from 'node:crypto';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import zlib from 'node:zlib';
import { windowLevel, type ImageInfo } from '@glcm/api';
import { PNG } from 'pngjs';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { ImageStore, newImageId } from '../src/storage/ImageStore.js';
import { createTestApp, encodeTiff, sixteenBitPattern, uploadImage, type TestApp } from './helpers.js';

const WIDTH = 50;
const HEIGHT = 30;

function nearestRank(values: number[], perMille: number): number {
  const sorted = [...values].sort((a, b) => a - b);
  return sorted[Math.max(1, Math.ceil((sorted.length * perMille) / 1000)) - 1];
}

describe('POST /images', () => {
  let t: TestApp;

  beforeEach(async () => {
    t = await createTestApp();
  });

  afterEach(async () => {
    await t.close();
  });

  it('stores a 16-bit TIFF and returns its info', async () => {
    const values = sixteenBitPattern(WIDTH, HEIGHT);
    const file = encodeTiff({ width: WIDTH, height: HEIGHT, bitsPerSample: 16, samplesPerPixel: 1, data: values });

    const response = await uploadImage(t.app, 'sample16.tif', file);
    expect(response.statusCode).toBe(201);
    const info = response.json<ImageInfo>();
    expect(info.imageId).toMatch(/^img_[0-9a-f]{32}$/);
    expect(info).toMatchObject({
      name: 'sample16.tif',
      sizeBytes: file.length,
      width: WIDTH,
      height: HEIGHT,
      bitDepth: 16,
      sourceChannels: 1,
      transfer: 'raw',
      warnings: [],
    });
    expect(info.sha256).toBe(createHash('sha256').update(file).digest('hex'));
    expect(info.windowMin).toBe(nearestRank(values, 5));
    expect(info.windowMax).toBe(nearestRank(values, 995));
    expect(info.histogram).toHaveLength(256);
    expect(info.histogram.reduce((sum, count) => sum + count, 0)).toBe(WIDTH * HEIGHT);

    const stored = await t.app.inject({ method: 'GET', url: `/api/v1/images/${info.imageId}` });
    expect(stored.statusCode).toBe(200);
    expect(stored.json()).toEqual(info);

    expect((await fs.readdir(path.join(t.dataDir, 'images', info.imageId))).sort()).toEqual(['info.json', 'original', 'pixels.bin']);
    expect(await fs.readdir(path.join(t.dataDir, 'uploads'))).toEqual([]);
  });

  it('uses server-side transfer for images above the raw limit', async () => {
    const small = await createTestApp({ rawTransferMaxPixels: 100 });
    try {
      const file = encodeTiff({ width: WIDTH, height: HEIGHT, bitsPerSample: 16, samplesPerPixel: 1, data: sixteenBitPattern(WIDTH, HEIGHT) });
      const response = await uploadImage(small.app, 'large.tif', file);
      expect(response.statusCode).toBe(201);
      expect(response.json<ImageInfo>().transfer).toBe('server');
    } finally {
      await small.close();
    }
  });

  it('rejects requests that are not multipart', async () => {
    const response = await t.app.inject({ method: 'POST', url: '/api/v1/images', payload: { hello: 'world' } });
    expect(response.statusCode).toBe(415);
    expect(response.json()).toMatchObject({ error: 'UnsupportedMediaType' });
  });

  it('rejects files that are not images', async () => {
    const response = await uploadImage(t.app, 'notes.png', Buffer.from('definitely not an image'));
    expect(response.statusCode).toBe(422);
    expect(response.json()).toMatchObject({ error: 'InvalidImage' });
    expect(await fs.readdir(path.join(t.dataDir, 'uploads'))).toEqual([]);
    expect(await fs.readdir(path.join(t.dataDir, 'images'))).toEqual([]);
  });

  it('rejects unsupported bit depths', async () => {
    const file = encodeTiff({ width: 2, height: 2, bitsPerSample: 32, samplesPerPixel: 1, sampleFormat: 'float', data: [0, 1, 2, 3] });
    const response = await uploadImage(t.app, 'float.tif', file);
    expect(response.statusCode).toBe(422);
    expect(response.json()).toMatchObject({ error: 'UnsupportedImage' });
  });

  it('rejects files above the upload limit', async () => {
    const limited = await createTestApp({ maxUploadBytes: 1000 });
    try {
      const file = encodeTiff({ width: WIDTH, height: HEIGHT, bitsPerSample: 16, samplesPerPixel: 1, data: sixteenBitPattern(WIDTH, HEIGHT) });
      const response = await uploadImage(limited.app, 'big.tif', file);
      expect(response.statusCode).toBe(413);
      expect(response.json()).toMatchObject({ error: 'PayloadTooLarge' });
      expect(await fs.readdir(path.join(limited.dataDir, 'uploads'))).toEqual([]);
    } finally {
      await limited.close();
    }
  });

  it('rejects images above the pixel limit', async () => {
    const limited = await createTestApp({ maxImagePixels: 100 });
    try {
      const file = encodeTiff({ width: WIDTH, height: HEIGHT, bitsPerSample: 16, samplesPerPixel: 1, data: sixteenBitPattern(WIDTH, HEIGHT) });
      const response = await uploadImage(limited.app, 'big.tif', file);
      expect(response.statusCode).toBe(422);
      expect(response.json()).toMatchObject({ error: 'ImageTooLarge' });
    } finally {
      await limited.close();
    }
  });

  it('rejects a huge image from its header without decoding it', async () => {
    // PNG signature and IHDR declaring 30000 × 30000 pixels, without image data: a decoder would need gigabytes
    const ihdr = Buffer.alloc(13);
    ihdr.writeUInt32BE(30000, 0);
    ihdr.writeUInt32BE(30000, 4);
    ihdr[8] = 16;
    const png = Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13]), Buffer.from('IHDR'), ihdr, Buffer.alloc(4)]);

    const response = await uploadImage(t.app, 'bomb.png', png);
    expect(response.statusCode).toBe(422);
    expect(response.json()).toMatchObject({ error: 'ImageTooLarge' });
    expect(response.json<{ message: string }>().message).toContain('30000 x 30000');
    expect(await fs.readdir(path.join(t.dataDir, 'uploads'))).toEqual([]);
  });
});

describe('image resources', () => {
  let t: TestApp;
  let info: ImageInfo;
  const values = sixteenBitPattern(WIDTH, HEIGHT);

  beforeEach(async () => {
    t = await createTestApp();
    const file = encodeTiff({ width: WIDTH, height: HEIGHT, bitsPerSample: 16, samplesPerPixel: 1, data: values });
    info = (await uploadImage(t.app, 'sample16.tif', file)).json<ImageInfo>();
  });

  afterEach(async () => {
    await t.close();
  });

  const url = (suffix = '') => `/api/v1/images/${info.imageId}${suffix}`;

  it('returns 404 for unknown images and 400 for malformed ids', async () => {
    const unknown = await t.app.inject({ method: 'GET', url: `/api/v1/images/img_${'0'.repeat(32)}` });
    expect(unknown.statusCode).toBe(404);
    expect(unknown.json()).toMatchObject({ error: 'NotFound' });

    const malformed = await t.app.inject({ method: 'GET', url: '/api/v1/images/..%2Fsecret' });
    expect(malformed.statusCode).toBe(400);
    expect(malformed.json()).toMatchObject({ error: 'BadRequest' });
  });

  it('deletes images', async () => {
    const deleted = await t.app.inject({ method: 'DELETE', url: url() });
    expect(deleted.statusCode).toBe(204);
    expect(deleted.rawPayload).toHaveLength(0);
    expect((await t.app.inject({ method: 'GET', url: url() })).statusCode).toBe(404);
    expect((await t.app.inject({ method: 'DELETE', url: url() })).statusCode).toBe(404);
    expect(await fs.readdir(path.join(t.dataDir, 'images'))).toEqual([]);
  });

  describe('GET /raw', () => {
    it('sends little-endian samples with size headers', async () => {
      const response = await t.app.inject({ method: 'GET', url: url('/raw'), headers: { 'accept-encoding': 'identity' } });
      expect(response.statusCode).toBe(200);
      expect(response.headers).toMatchObject({
        'content-type': 'application/octet-stream',
        'x-image-width': String(WIDTH),
        'x-image-height': String(HEIGHT),
        'x-image-bit-depth': '16',
        'x-image-byte-order': 'little-endian',
        'cache-control': 'private, max-age=31536000, immutable',
        vary: 'Accept-Encoding',
      });
      expect(response.headers['content-encoding']).toBeUndefined();

      const body = response.rawPayload;
      expect(body).toHaveLength(WIDTH * HEIGHT * 2);
      for (let i = 0; i < values.length; i += 1) {
        expect(body.readUInt16LE(2 * i)).toBe(values[i]);
      }
    });

    it('compresses with gzip and reuses the compressed copy', async () => {
      const identity = (await t.app.inject({ method: 'GET', url: url('/raw'), headers: { 'accept-encoding': 'identity' } })).rawPayload;
      const response = await t.app.inject({ method: 'GET', url: url('/raw'), headers: { 'accept-encoding': 'gzip, deflate' } });
      expect(response.statusCode).toBe(200);
      expect(response.headers['content-encoding']).toBe('gzip');
      expect(zlib.gunzipSync(response.rawPayload).equals(identity)).toBe(true);
      await expect(fs.access(path.join(t.dataDir, 'images', info.imageId, 'pixels.bin.gzip'))).resolves.toBeUndefined();

      const again = await t.app.inject({ method: 'GET', url: url('/raw'), headers: { 'accept-encoding': 'gzip' } });
      expect(again.rawPayload.equals(response.rawPayload)).toBe(true);
    });

    it.skipIf(typeof zlib.zstdDecompressSync !== 'function')('prefers zstd when accepted', async () => {
      const identity = (await t.app.inject({ method: 'GET', url: url('/raw'), headers: { 'accept-encoding': 'identity' } })).rawPayload;
      const response = await t.app.inject({ method: 'GET', url: url('/raw'), headers: { 'accept-encoding': 'gzip, br, zstd' } });
      expect(response.headers['content-encoding']).toBe('zstd');
      expect(zlib.zstdDecompressSync(response.rawPayload).equals(identity)).toBe(true);

      const excluded = await t.app.inject({ method: 'GET', url: url('/raw'), headers: { 'accept-encoding': 'zstd;q=0, gzip' } });
      expect(excluded.headers['content-encoding']).toBe('gzip');
    });

    it('answers conditional requests with 304', async () => {
      const first = await t.app.inject({ method: 'GET', url: url('/raw'), headers: { 'accept-encoding': 'gzip' } });
      const etag = first.headers.etag as string;
      expect(etag).toBe(`"${info.sha256}-gzip"`);

      const second = await t.app.inject({ method: 'GET', url: url('/raw'), headers: { 'accept-encoding': 'gzip', 'if-none-match': etag } });
      expect(second.statusCode).toBe(304);
      expect(second.rawPayload).toHaveLength(0);
    });

    it('is not available for images above the raw limit', async () => {
      const limited = await createTestApp({ rawTransferMaxPixels: 100 });
      try {
        const file = encodeTiff({ width: WIDTH, height: HEIGHT, bitsPerSample: 16, samplesPerPixel: 1, data: values });
        const large = (await uploadImage(limited.app, 'large.tif', file)).json<ImageInfo>();
        const response = await limited.app.inject({ method: 'GET', url: `/api/v1/images/${large.imageId}/raw` });
        expect(response.statusCode).toBe(409);
        expect(response.json()).toMatchObject({ error: 'RawNotAvailable' });
      } finally {
        await limited.close();
      }
    });
  });

  describe('GET /display.png', () => {
    it('renders the default window', async () => {
      const response = await t.app.inject({ method: 'GET', url: url('/display.png') });
      expect(response.statusCode).toBe(200);
      expect(response.headers['content-type']).toBe('image/png');
      expect(response.headers['cache-control']).toBe('private, max-age=86400');

      const png = PNG.sync.read(response.rawPayload);
      expect([png.width, png.height]).toEqual([WIDTH, HEIGHT]);
      for (let i = 0; i < values.length; i += 1) {
        expect(png.data[i * 4]).toBe(windowLevel(values[i], info.windowMin, info.windowMax));
      }
    });

    it('applies a custom window and downscales', async () => {
      const custom = PNG.sync.read((await t.app.inject({ method: 'GET', url: url('/display.png?min=1000&max=40000') })).rawPayload);
      expect(custom.data[7 * 4]).toBe(windowLevel(values[7], 1000, 40000));

      const small = PNG.sync.read((await t.app.inject({ method: 'GET', url: url('/display.png?maxSize=25') })).rawPayload);
      expect([small.width, small.height]).toEqual([25, 15]);
    });

    it('uses ETags that depend on the rendering parameters', async () => {
      const first = await t.app.inject({ method: 'GET', url: url('/display.png?min=0&max=65535') });
      const etag = first.headers.etag as string;
      const cached = await t.app.inject({ method: 'GET', url: url('/display.png?min=0&max=65535'), headers: { 'if-none-match': etag } });
      expect(cached.statusCode).toBe(304);
      expect(cached.rawPayload).toHaveLength(0);

      const other = await t.app.inject({ method: 'GET', url: url('/display.png?min=0&max=65534') });
      expect(other.headers.etag).not.toBe(etag);
      const again = await t.app.inject({ method: 'GET', url: url('/display.png?min=0&max=65535') });
      expect(again.rawPayload.equals(first.rawPayload)).toBe(true);
    });

    it('validates the window', async () => {
      const inverted = await t.app.inject({ method: 'GET', url: url('/display.png?min=5000&max=10') });
      expect(inverted.statusCode).toBe(400);
      expect(inverted.json()).toMatchObject({ error: 'BadRequest' });
      expect((await t.app.inject({ method: 'GET', url: url('/display.png?max=70000') })).statusCode).toBe(400);
      expect((await t.app.inject({ method: 'GET', url: url('/display.png?min=abc') })).statusCode).toBe(400);
    });
  });

  describe('GET /pixel', () => {
    it('returns one stored value', async () => {
      const response = await t.app.inject({ method: 'GET', url: url('/pixel?x=7&y=3') });
      expect(response.statusCode).toBe(200);
      expect(response.headers['cache-control']).toBe('no-store');
      expect(response.json()).toEqual({ x: 7, y: 3, value: values[3 * WIDTH + 7] });
    });

    it('rejects positions outside the image', async () => {
      expect((await t.app.inject({ method: 'GET', url: url(`/pixel?x=${WIDTH}&y=0`) })).statusCode).toBe(400);
      expect((await t.app.inject({ method: 'GET', url: url('/pixel?x=-1&y=0') })).statusCode).toBe(400);
      expect((await t.app.inject({ method: 'GET', url: url('/pixel?x=1') })).statusCode).toBe(400);
    });
  });
});

describe('pixel cache', () => {
  let dataDir: string;

  beforeEach(async () => {
    dataDir = await fs.mkdtemp(path.join(os.tmpdir(), 'glcm-pixel-cache-'));
  });

  afterEach(async () => {
    await fs.rm(dataDir, { recursive: true, force: true });
  });

  /** A store with images whose 60 pixel bytes all have the value 1, 2, ... */
  async function storeWith(cacheBytes: number, count: number): Promise<{ store: ImageStore; ids: string[] }> {
    const store = new ImageStore(dataDir, cacheBytes);
    await store.init();
    const ids: string[] = [];
    for (let i = 0; i < count; i += 1) {
      const upload = store.temporaryUploadPath();
      await fs.writeFile(upload, 'original');
      const imageId = newImageId();
      await store.save({ imageId } as ImageInfo, Buffer.alloc(60, i + 1), upload);
      ids.push(imageId);
    }
    return { store, ids };
  }

  it('shares pixel buffers between requests and evicts the least recently used', async () => {
    const {
      store,
      ids: [first, second],
    } = await storeWith(100, 2);
    const [a, b] = await Promise.all([store.pixels(first), store.pixels(first)]);
    expect(a).toBe(b);
    expect(a[0]).toBe(1);
    expect(store.pixelCacheSize).toBe(60);
    expect(await store.pixels(first)).toBe(a);

    expect((await store.pixels(second))[0]).toBe(2);
    // 120 bytes do not fit into 100: the first image was evicted
    expect(store.pixelCacheSize).toBe(60);
    const reread = await store.pixels(first);
    expect(reread).not.toBe(a);
    expect(reread).toEqual(a);
  });

  it('forgets removed images and can be disabled', async () => {
    const {
      store,
      ids: [id],
    } = await storeWith(1000, 1);
    await store.pixels(id);
    expect(store.pixelCacheSize).toBe(60);
    expect(await store.remove(id)).toBe(true);
    expect(store.pixelCacheSize).toBe(0);
    await expect(store.pixels(id)).rejects.toMatchObject({ code: 'ENOENT' });
    expect(store.pixelCacheSize).toBe(0);

    const {
      store: uncached,
      ids: [other],
    } = await storeWith(0, 1);
    expect(await uncached.pixels(other)).not.toBe(await uncached.pixels(other));
    expect(uncached.pixelCacheSize).toBe(0);
  });
});
