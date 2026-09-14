// Writes packages/api/openapi.json from the route schemas: npm run openapi
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { buildApp } from '../src/app.js';
import { DEFAULT_CONFIG } from '../src/config.js';

const dataDir = await fs.mkdtemp(path.join(os.tmpdir(), 'glcm-openapi-'));
try {
  const app = await buildApp({ ...DEFAULT_CONFIG, dataDir, webDir: null, samplesDir: null }, { logger: false });
  await app.ready();
  const output = path.resolve(import.meta.dirname, '../../packages/api/openapi.json');
  await fs.writeFile(output, `${JSON.stringify(app.swagger(), null, 2)}\n`);
  await app.close();
  console.log(`Wrote ${path.relative(process.cwd(), output)}`);
} finally {
  await fs.rm(dataDir, { recursive: true, force: true });
}
