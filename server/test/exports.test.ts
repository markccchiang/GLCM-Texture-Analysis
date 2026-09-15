import type { AnalysisInfo, AnalysisResults, AnalysisSettings, ImageInfo, ImageListResponse, ResultsDocument } from '@glcm/api';
import { unzipSync } from 'fflate';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { createTestApp, encodeTiff, uploadImage, type TestApp } from './helpers.js';

// Haralick, Shanmugam and Dinstein (1973), figure 2
const HARALICK = [0, 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 2, 2, 2, 3, 3];
const TIFF = encodeTiff({ width: 4, height: 4, bitsPerSample: 8, samplesPerPixel: 1, data: HARALICK });

const SETTINGS: AnalysisSettings = {
  features: ['Mean', 'Contrast', 'CorrelationIII'],
  grayLevels: 4,
  quantization: { method: 'none', min: 0, max: 255, binWidth: 0 },
  distances: [1],
  directions: [0, 45, 90, 135],
  aggregation: 'perDirectionAndMean',
  logBase: 'natural',
  score: { enabled: false, age: 40, coefficients: [1.138, -1.814, 1.416, 1.714], profile: 'calibration', intensityMin: 0, intensityMax: 255 },
};

const WHOLE = { id: 'whole', name: 'Whole', shape: { type: 'rectangle' as const, x: 0, y: 0, width: 4, height: 4 } };
const TOP_LEFT = { id: 'top-left', name: 'Top left', color: '#FFD400', shape: { type: 'rectangle' as const, x: 0, y: 0, width: 2, height: 2 } };

function csvRows(text: string): string[][] {
  return text
    .trimEnd()
    .split('\n')
    .filter((line) => !line.startsWith('#'))
    .map((line) => line.split(','));
}

describe('exports', () => {
  let t: TestApp;
  let image: ImageInfo;
  let analysis: AnalysisResults;

  beforeAll(async () => {
    t = await createTestApp({ analysisConcurrency: 2 });
    image = (await uploadImage(t.app, 'haralick.tif', TIFF)).json<ImageInfo>();
    const started = await t.app.inject({ method: 'POST', url: '/api/v1/analyses', payload: { imageId: image.imageId, rois: [WHOLE, TOP_LEFT], settings: SETTINGS } });
    const { analysisId } = started.json<AnalysisInfo>();
    await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${analysisId}/events` });
    analysis = (await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${analysisId}/results` })).json<AnalysisResults>();
  });

  afterAll(async () => {
    await t.close();
  });

  const documentOf = (results: AnalysisResults, settings: AnalysisSettings = results.settings): ResultsDocument => ({
    timestamp: results.timestamp,
    image: { name: results.image.name, sha256: results.image.sha256 },
    settings,
    results: results.results,
  });

  const exportResults = (format: 'csv' | 'json', documents: ResultsDocument[]) =>
    t.app.inject({ method: 'POST', url: '/api/v1/exports/results', payload: { format, documents } });

  it('exports one analysis as CSV with the settings header', async () => {
    const response = await exportResults('csv', [documentOf(analysis)]);
    expect(response.statusCode).toBe(200);
    expect(response.headers['content-type']).toBe('text/csv; charset=utf-8');
    expect(response.headers['content-disposition']).toContain('filename="haralick-results.csv"');
    expect(response.body).toContain('# format=glcm-results-csv');
    expect(response.body).toContain(`# imageSha256=${image.sha256}`);

    const rows = csvRows(response.body);
    const header = rows[0];
    expect(header).toContain('Correlation III [non-standard]');
    expect(rows).toHaveLength(1 + 10);
    const contrast = header.indexOf('Contrast');
    expect(Number(rows[1][contrast])).toBeCloseTo(14 / 24, 12);
    expect(rows[6][header.indexOf('roiName')]).toBe('Top left');
  });

  it('exports canonical JSON that reads back unchanged', async () => {
    const response = await exportResults('json', [documentOf(analysis)]);
    expect(response.headers['content-type']).toBe('application/json; charset=utf-8');
    const document = response.json<AnalysisResults & { coreVersion: string }>();
    expect(document).toMatchObject({ format: 'glcm-results', version: 1, coreVersion: '0.1.0', settings: SETTINGS });
    expect(document.results).toEqual(analysis.results);

    const again = await exportResults('json', [{ timestamp: document.timestamp, image: document.image, settings: document.settings, results: document.results }]);
    expect(again.body).toBe(response.body);
  });

  it('merges documents with the same settings and zips different ones', async () => {
    const merged = await exportResults('csv', [documentOf(analysis), documentOf(analysis)]);
    expect(merged.headers['content-type']).toBe('text/csv; charset=utf-8');
    expect(csvRows(merged.body)).toHaveLength(1 + 20);

    const other = { ...SETTINGS, aggregation: 'meanOnly' as const };
    const zipped = await exportResults('json', [documentOf(analysis), documentOf(analysis, other)]);
    expect(zipped.headers['content-type']).toBe('application/zip');
    expect(zipped.headers['content-disposition']).toContain('results.zip');
    const files = unzipSync(zipped.rawPayload);
    expect(Object.keys(files)).toEqual(['1-haralick-results.json', '2-haralick-results.json']);
    expect(JSON.parse(new TextDecoder().decode(files['2-haralick-results.json'])).settings.aggregation).toBe('meanOnly');
  });

  it('keeps documents with different pixel spacings apart', async () => {
    const plain = documentOf(analysis);
    const spaced = { ...plain, image: { ...plain.image, pixelSpacing: { x: 0.5, y: 0.5 } } };
    const response = await exportResults('csv', [plain, spaced]);
    expect(response.headers['content-type']).toBe('application/zip');
    const files = unzipSync(response.rawPayload);
    const second = new TextDecoder().decode(files['2-haralick-results.csv']);
    expect(second).toContain('# pixelSpacingMm=0.5;0.5');
    expect(second).toContain(',pixelCount,areaMm2,');
    expect(new TextDecoder().decode(files['1-haralick-results.csv'])).not.toContain('areaMm2');
  });

  it('rejects invalid documents', async () => {
    const broken = { ...documentOf(analysis), results: [{ ...analysis.results[0], values: { NoSuchFeature: analysis.results[0].values.Contrast } }] };
    const response = await exportResults('csv', [broken]);
    expect(response.statusCode).toBe(400);
    expect(response.json().message).toContain('NoSuchFeature');
    expect((await exportResults('csv', [])).statusCode).toBe(400);
  });

  it('serves analysis results as CSV and JSON files', async () => {
    const csv = await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${analysis.analysisId}/results.csv` });
    expect(csv.statusCode).toBe(200);
    expect(csv.headers['content-disposition']).toContain('haralick-results.csv');
    expect(csvRows(csv.body)).toHaveLength(11);
    const json = await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${analysis.analysisId}/results.json` });
    expect(json.json().results).toEqual(analysis.results);
  });

  it('exports ROI images as a ZIP', async () => {
    const response = await t.app.inject({
      method: 'POST',
      url: '/api/v1/exports/roi-images',
      payload: { imageId: image.imageId, rois: [WHOLE, TOP_LEFT], settings: SETTINGS, transparentOutside: false, includeQuantized: true },
    });
    expect(response.statusCode).toBe(200);
    expect(response.headers['content-disposition']).toContain('haralick-rois.zip');
    const files = unzipSync(response.rawPayload);
    expect(Object.keys(files).sort()).toEqual(['Top_left.png', 'Top_left_mask.png', 'Top_left_q4.png', 'Whole.png', 'Whole_mask.png', 'Whole_q4.png', 'manifest.json']);
    const manifest = JSON.parse(new TextDecoder().decode(files['manifest.json']));
    expect(manifest.entries.map((entry: { pixelCount: number }) => entry.pixelCount)).toEqual([16, 4]);

    const noSettings = await t.app.inject({
      method: 'POST',
      url: '/api/v1/exports/roi-images',
      payload: { imageId: image.imageId, rois: [WHOLE], transparentOutside: false, includeQuantized: true },
    });
    expect(noSettings.statusCode).toBe(400);
    const missing = await t.app.inject({
      method: 'POST',
      url: '/api/v1/exports/roi-images',
      payload: { imageId: `img_${'0'.repeat(32)}`, rois: [WHOLE], transparentOutside: false, includeQuantized: false },
    });
    expect(missing.statusCode).toBe(404);
  });

  it('finds images by content and serves the uploaded file', async () => {
    const found = await t.app.inject({ method: 'GET', url: `/api/v1/images?sha256=${image.sha256}` });
    expect(found.json<ImageListResponse>().images.map((info) => info.imageId)).toEqual([image.imageId]);
    const none = await t.app.inject({ method: 'GET', url: `/api/v1/images?sha256=${'f'.repeat(64)}` });
    expect(none.json<ImageListResponse>().images).toEqual([]);
    expect((await t.app.inject({ method: 'GET', url: '/api/v1/images?sha256=xyz' })).statusCode).toBe(400);

    const original = await t.app.inject({ method: 'GET', url: `/api/v1/images/${image.imageId}/original` });
    expect(original.statusCode).toBe(200);
    expect(original.headers['content-disposition']).toContain('filename="haralick.tif"');
    expect(original.rawPayload.equals(TIFF)).toBe(true);
  });
});
