import { randomUUID } from 'node:crypto';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { buildApp, type App } from '../src/app.js';
import { DEFAULT_CONFIG, type ServerConfig } from '../src/config.js';

export { encodeTiff } from '../../bindings/node/test/tiff.js';

export interface TestApp {
  app: App;
  config: ServerConfig;
  dataDir: string;
  close(): Promise<void>;
}

/** App with its own temporary data directory */
export async function createTestApp(overrides: Partial<ServerConfig> = {}): Promise<TestApp> {
  const dataDir = await fs.mkdtemp(path.join(os.tmpdir(), 'glcm-server-test-'));
  const config: ServerConfig = { ...DEFAULT_CONFIG, dataDir, logLevel: 'silent', webDir: null, samplesDir: null, ...overrides };
  const app = await buildApp(config, { logger: false });
  return {
    app,
    config,
    dataDir,
    async close() {
      await app.close();
      await fs.rm(dataDir, { recursive: true, force: true });
    },
  };
}

export function multipartBody(fileName: string, data: Buffer, contentType = 'image/tiff') {
  const boundary = `----glcm${randomUUID()}`;
  const head = Buffer.from(
    `--${boundary}\r\nContent-Disposition: form-data; name="file"; filename="${fileName}"\r\nContent-Type: ${contentType}\r\n\r\n`,
  );
  const tail = Buffer.from(`\r\n--${boundary}--\r\n`);
  return {
    payload: Buffer.concat([head, data, tail]),
    headers: { 'content-type': `multipart/form-data; boundary=${boundary}` },
  };
}

export function uploadImage(app: App, fileName: string, data: Buffer) {
  const { payload, headers } = multipartBody(fileName, data);
  return app.inject({ method: 'POST', url: '/api/v1/images', payload, headers });
}

/** 16-bit samples with a spread of values */
export function sixteenBitPattern(width: number, height: number): number[] {
  return Array.from({ length: width * height }, (_, i) => (i * 977 + Math.floor(i / width) * 131) % 65536);
}
