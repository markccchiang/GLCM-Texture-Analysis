// Smoke test of a running server, e.g. the Docker image: node scripts/smoke-test.mjs [baseUrl] [imageFile]
// Uses GLCM_API_TOKEN when the server requires a token. Needs only Node.js (no npm packages).

import { readFile } from 'node:fs/promises';
import path from 'node:path';

const baseUrl = (process.argv[2] ?? 'http://127.0.0.1:8080').replace(/\/$/, '');
const imagePath = process.argv[3] ?? path.join(import.meta.dirname, '..', 'samples', 'textures', 'brick.png');
const token = process.env.GLCM_API_TOKEN ?? '';
const api = `${baseUrl}/api/v1`;

// Every request, including reading a response body such as the event stream, fails after this time instead of hanging
const REQUEST_TIMEOUT_MS = 120_000;
const fetch = (url, options = {}) => globalThis.fetch(url, { signal: AbortSignal.timeout(REQUEST_TIMEOUT_MS), ...options });

let failures = 0;
function check(condition, message) {
  console.log(`${condition ? '✓' : '✗'} ${message}`);
  if (!condition) {
    failures += 1;
  }
  return condition;
}

const headers = (extra = {}) => ({ ...(token ? { authorization: `Bearer ${token}` } : {}), ...extra });

async function main() {
  const health = await (await fetch(`${api}/health`)).json();
  check(health.status === 'ok', `health: core ${health.coreVersion}, ${health.mode} mode, authentication ${health.authentication}`);

  if (health.authentication === 'bearer') {
    const denied = await fetch(`${api}/catalog`);
    check(denied.status === 401 && denied.headers.get('www-authenticate') === 'Bearer', 'the API rejects requests without the token');
    check((await fetch(`${api}/catalog`, { headers: { authorization: 'Bearer wrong' } })).status === 401, 'the API rejects a wrong token');
    if (!check(token !== '', 'GLCM_API_TOKEN is set for the remaining checks')) {
      return;
    }
  }

  const page = await fetch(`${baseUrl}/`, { headers: { accept: 'text/html' } });
  check(page.ok && (await page.text()).includes('<div id="root">'), 'the web app is served');

  const catalog = await (await fetch(`${api}/catalog`, { headers: headers() })).json();
  check(Array.isArray(catalog.features) && catalog.features.length > 0, `catalog: ${catalog.features?.length} features`);

  const form = new FormData();
  form.append('file', new Blob([await readFile(imagePath)]), path.basename(imagePath));
  const uploaded = await fetch(`${api}/images`, { method: 'POST', body: form, headers: headers() });
  const image = await uploaded.json();
  if (!check(uploaded.status === 201, `upload ${path.basename(imagePath)}: ${image.width}×${image.height} ${image.bitDepth}-bit`)) {
    return;
  }

  const settings = {
    features: ['Mean', 'Contrast', 'Entropy'],
    grayLevels: 32,
    quantization: { method: 'fixedRange', min: 0, max: image.bitDepth === 16 ? 65535 : 255, binWidth: 8 },
    distances: [1, 2],
    directions: [0, 45, 90, 135],
    aggregation: 'perDirectionAndMean',
    logBase: 'natural',
    score: { enabled: true, age: 40, coefficients: [1.138, -1.814, 1.416, 1.714], profile: 'calibration', intensityMin: 0, intensityMax: 255 },
  };
  const rois = [
    { id: 'r1', name: 'Rectangle', shape: { type: 'rectangle', x: 10, y: 10, width: 100, height: 80 } },
    { id: 'r2', name: 'Ellipse', shape: { type: 'ellipse', cx: 200, cy: 150, rx: 60, ry: 40, angle: 20 } },
  ];
  const started = await fetch(`${api}/analyses`, {
    method: 'POST',
    headers: headers({ 'content-type': 'application/json' }),
    body: JSON.stringify({ imageId: image.imageId, rois, settings }),
  });
  const analysis = await started.json();
  if (!check(started.status === 202, `analysis started: ${analysis.total} jobs`)) {
    return;
  }

  const events = await fetch(`${api}/analyses/${analysis.analysisId}/events`, { headers: headers() });
  const stream = await events.text();
  check(stream.includes('event: finished') && stream.includes('"status":"completed"'), 'progress stream finished with status completed');

  const csv = await fetch(`${api}/analyses/${analysis.analysisId}/results.csv`, { headers: headers() });
  const lines = (await csv.text()).trim().split('\n').filter((line) => !line.startsWith('#'));
  check(csv.ok && lines.length === 1 + 2 * 2 * 5, `results.csv: ${lines.length - 1} rows`);

  const deleted = await fetch(`${api}/images/${image.imageId}`, { method: 'DELETE', headers: headers() });
  check(deleted.status === 204, 'uploaded image deleted');
}

try {
  await main();
} catch (error) {
  check(false, `unexpected error: ${error instanceof Error ? error.message : error}`);
}
console.log(failures === 0 ? 'Smoke test passed' : `Smoke test failed (${failures})`);
process.exit(failures === 0 ? 0 : 1);
