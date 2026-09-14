// Shared helpers of the end-to-end tests. The app is opened with ?testHooks, which exposes its stores as window.__glcm.

import path from 'node:path';
import { expect, type Page } from '@playwright/test';
import type { AnalysisSettings, RoiShape } from '@glcm/api';

export const ROOT = path.resolve(import.meta.dirname, '..');

export const SETTINGS: AnalysisSettings = {
  // Catalog order, which exported settings use
  features: ['Mean', 'Energy', 'Contrast', 'CorrelationII', 'Entropy', 'CorrelationIII'],
  grayLevels: 32,
  quantization: { method: 'fixedRange', min: 0, max: 255, binWidth: 8 },
  distances: [1],
  directions: [0, 45, 90, 135],
  aggregation: 'perDirectionAndMean',
  logBase: 'natural',
  score: { enabled: false, age: 40, coefficients: [1.138, -1.814, 1.416, 1.714], profile: 'calibration', intensityMin: 0, intensityMax: 255 },
};

export interface Viewport {
  scale: number;
  x: number;
  y: number;
}

export interface StoredRoi {
  id: string;
  name: string;
  color: string;
  visible: boolean;
  shape: RoiShape;
}

export interface StoredRow {
  roiId: string;
  roiName: string;
  distance: number;
  direction: string | null;
  status: string;
  values: Record<string, number | null>;
}

interface Hooks {
  viewer: { getState(): { viewport: Viewport; image: { info: { imageId: string; sha256: string } } | null } };
  rois: { getState(): { rois: StoredRoi[] } };
  results: { getState(): { rows: StoredRow[] } };
  settings: { getState(): { settings: AnalysisSettings | null } };
}

/** Opens the app with the test settings and the lena sample */
export async function openSample(page: Page): Promise<void> {
  await page.addInitScript((settings) => {
    window.localStorage.setItem('glcm.analysisSettings', JSON.stringify({ state: { settings }, version: 1 }));
  }, SETTINGS);
  await page.goto('/?testHooks');
  await page.getByRole('button', { name: 'Open sample image' }).click();
  await waitForImage(page);
}

export async function waitForImage(page: Page): Promise<void> {
  const status = page.getByTestId('status-bar');
  await expect(status).toContainText('lena.jpg 650×366 8-bit');
  await expect(status).toContainText(/WebGL2|Lookup table|Server rendering/);
}

export const viewport = (page: Page) => page.evaluate(() => (window as unknown as { __glcm: Hooks }).__glcm.viewer.getState().viewport);
export const openImage = (page: Page) => page.evaluate(() => (window as unknown as { __glcm: Hooks }).__glcm.viewer.getState().image?.info ?? null);
export const storedRois = (page: Page) => page.evaluate(() => (window as unknown as { __glcm: Hooks }).__glcm.rois.getState().rois);
export const resultRows = (page: Page) => page.evaluate(() => (window as unknown as { __glcm: Hooks }).__glcm.results.getState().rows);
export const storedSettings = (page: Page) => page.evaluate(() => (window as unknown as { __glcm: Hooks }).__glcm.settings.getState().settings);

/** Page coordinates of an image point */
export async function toPage(page: Page, x: number, y: number): Promise<{ x: number; y: number }> {
  const box = (await page.getByTestId('image-canvas').boundingBox())!;
  const { scale, x: offsetX, y: offsetY } = await viewport(page);
  return { x: box.x + offsetX + x * scale, y: box.y + offsetY + y * scale };
}

export async function drag(page: Page, from: [number, number], to: [number, number]): Promise<void> {
  const start = await toPage(page, ...from);
  const end = await toPage(page, ...to);
  await page.mouse.move(start.x, start.y);
  await page.mouse.down();
  await page.mouse.move(end.x, end.y, { steps: 8 });
  await page.mouse.up();
}

export async function clickAt(page: Page, x: number, y: number): Promise<void> {
  const point = await toPage(page, x, y);
  await page.mouse.click(point.x, point.y);
}

/** Draws and adds a rectangle, an ellipse and a polygon */
export async function drawThreeRois(page: Page): Promise<void> {
  await page.keyboard.press('r');
  await drag(page, [100, 100], [164, 150]);
  await page.keyboard.press('t');
  await page.keyboard.press('e');
  await drag(page, [200, 150], [280, 210]);
  await page.keyboard.press('t');
  await page.keyboard.press('p');
  for (const [x, y] of [
    [350, 100],
    [450, 120],
    [420, 220],
  ]) {
    await clickAt(page, x, y);
  }
  await page.keyboard.press('Enter');
  await page.keyboard.press('t');
  await expect(page.getByTestId('roi-row')).toHaveCount(3);
}

/** RFC 4180 CSV with "#" comment lines before the header, as written by glcm::ResultsToCsv */
export function parseCsv(text: string): { comments: string[]; header: string[]; rows: string[][] } {
  const lines = text.split('\n');
  const comments: string[] = [];
  while (lines.length > 0 && lines[0].startsWith('#')) {
    comments.push(lines.shift()!);
  }
  const records: string[][] = [];
  let record: string[] = [];
  let field = '';
  let quoted = false;
  const body = lines.join('\n');
  for (let i = 0; i < body.length; i += 1) {
    const c = body[i];
    if (quoted) {
      if (c === '"' && body[i + 1] === '"') {
        field += '"';
        i += 1;
      } else if (c === '"') {
        quoted = false;
      } else {
        field += c;
      }
    } else if (c === '"') {
      quoted = true;
    } else if (c === ',') {
      record.push(field);
      field = '';
    } else if (c === '\n') {
      record.push(field);
      records.push(record);
      record = [];
      field = '';
    } else {
      field += c;
    }
  }
  if (field !== '' || record.length > 0) {
    record.push(field);
    records.push(record);
  }
  const [header = [], ...rows] = records;
  return { comments, header, rows };
}

/** Clicks an item of a main menu */
export async function chooseMenuItem(page: Page, menu: string, item: string | RegExp): Promise<void> {
  await page.getByRole('navigation', { name: 'Main menu' }).getByRole('button', { name: menu, exact: true }).click();
  await page.getByRole('menuitem', { name: item }).click();
}
