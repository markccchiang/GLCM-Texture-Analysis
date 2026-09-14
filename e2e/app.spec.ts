// End-to-end tests of R1–R3 in local mode (doc/ui-design-plan.md, section 8.3). Expected values come from the glcm_core
// addon, run on the same image, ROI geometry and settings.

import path from 'node:path';
import { expect, test, type Page } from '@playwright/test';
import type { AnalysisSettings, RoiShape } from '@glcm/api';
import * as native from '@glcm/native';

const ROOT = path.resolve(import.meta.dirname, '..');

const SETTINGS: AnalysisSettings = {
  features: ['Mean', 'Energy', 'Contrast', 'Entropy', 'CorrelationII'],
  grayLevels: 32,
  quantization: { method: 'fixedRange', min: 0, max: 255, binWidth: 8 },
  distances: [1],
  directions: [0, 45, 90, 135],
  aggregation: 'perDirectionAndMean',
  logBase: 'natural',
  score: { enabled: false, age: 40, coefficients: [1.138, -1.814, 1.416, 1.714], profile: 'calibration', intensityMin: 0, intensityMax: 255 },
};

interface Viewport {
  scale: number;
  x: number;
  y: number;
}

interface StoredRoi {
  id: string;
  name: string;
  shape: RoiShape;
}

type Hooks = {
  viewer: { getState(): { viewport: Viewport } };
  rois: { getState(): { rois: StoredRoi[]; selectedIds: string[] } };
  results: { getState(): { rows: Array<{ roiId: string; direction: string | null; status: string; values: Record<string, number | null> }> } };
};

let lena: native.DecodedImage;

test.beforeAll(async () => {
  lena = await native.decodeImageFile(path.join(ROOT, 'samples', 'lena.jpg'));
});

test.beforeEach(async ({ page }) => {
  await page.addInitScript((settings) => {
    window.localStorage.setItem('glcm.analysisSettings', JSON.stringify({ state: { settings }, version: 1 }));
  }, SETTINGS);
  await page.goto('/?testHooks');
  await page.getByRole('button', { name: 'Open sample image' }).click();
  const status = page.getByTestId('status-bar');
  await expect(status).toContainText('lena.jpg 650×366 8-bit');
  await expect(status).toContainText(/WebGL2|Lookup table|Server rendering/);
});

const viewport = (page: Page) => page.evaluate(() => (window as unknown as { __glcm: Hooks }).__glcm.viewer.getState().viewport);
const storedRois = (page: Page) => page.evaluate(() => (window as unknown as { __glcm: Hooks }).__glcm.rois.getState().rois);
const resultRows = (page: Page) => page.evaluate(() => (window as unknown as { __glcm: Hooks }).__glcm.results.getState().rows);

/** Page coordinates of an image point */
async function toPage(page: Page, x: number, y: number): Promise<{ x: number; y: number }> {
  const box = (await page.getByTestId('image-canvas').boundingBox())!;
  const { scale, x: offsetX, y: offsetY } = await viewport(page);
  return { x: box.x + offsetX + x * scale, y: box.y + offsetY + y * scale };
}

async function drag(page: Page, from: [number, number], to: [number, number]): Promise<void> {
  const start = await toPage(page, ...from);
  const end = await toPage(page, ...to);
  await page.mouse.move(start.x, start.y);
  await page.mouse.down();
  await page.mouse.move(end.x, end.y, { steps: 8 });
  await page.mouse.up();
}

async function clickAt(page: Page, x: number, y: number): Promise<void> {
  const point = await toPage(page, x, y);
  await page.mouse.click(point.x, point.y);
}

async function expectedPixelCounts(rois: StoredRoi[]): Promise<number[]> {
  const stats = await native.roiStats(lena.pixels, lena.width, lena.height, lena.bitDepth, JSON.stringify(rois));
  return stats.map((s) => s.pixelCount);
}

test('measures a rectangle ROI with the values of the core', async ({ page }) => {
  await page.keyboard.press('r');
  await drag(page, [100, 100], [164, 164]);
  await page.keyboard.press('t');

  const row = page.getByTestId('roi-row');
  await expect(row).toHaveCount(1);
  await expect(row).toContainText('ROI 1');
  const [roi] = await storedRois(page);
  // About 64 × 64 px; WebKit rounds mouse positions to whole CSS pixels, which moves the edges slightly
  const [pixels] = await expectedPixelCounts([roi]);
  expect(pixels).toBeGreaterThan(3600);
  await expect(row).toContainText(`${pixels.toLocaleString('en-US')} px`);

  await page.keyboard.press('m');
  const table = page.getByTestId('results-table');
  await expect(table.locator('tbody tr')).toHaveCount(5);

  const expected = JSON.parse(
    await native.runAnalysis(lena.pixels, lena.width, lena.height, lena.bitDepth, JSON.stringify([{ id: roi.id, name: roi.name, shape: roi.shape }]), JSON.stringify(SETTINGS)),
  ).results[0];
  const rows = await resultRows(page);
  expect(rows.map((r) => r.direction)).toEqual(['0', '45', '90', '135', 'mean']);
  for (const r of rows) {
    expect(r.status).toBe('ok');
    for (const feature of SETTINGS.features) {
      expect(r.values[feature], `${feature} ${r.direction}`).toBe(expected.values[feature][r.direction!]);
    }
  }
  await expect(table).toContainText(String(Number(expected.values.Contrast.mean.toPrecision(6))));
});

test('draws ellipse, polygon and freehand ROIs and measures all', async ({ page }) => {
  await page.keyboard.press('e');
  await drag(page, [200, 150], [280, 210]);
  await page.keyboard.press('t');

  await page.keyboard.press('p');
  for (const [x, y] of [
    [350, 100],
    [450, 120],
    [420, 220],
    [360, 200],
  ]) {
    await clickAt(page, x, y);
  }
  await page.keyboard.press('Enter');
  await page.keyboard.press('t');

  await page.keyboard.press('f');
  const path = [
    [500, 250],
    [560, 240],
    [600, 290],
    [560, 340],
    [500, 320],
  ];
  const first = await toPage(page, path[0][0], path[0][1]);
  await page.mouse.move(first.x, first.y);
  await page.mouse.down();
  for (const [x, y] of path.slice(1)) {
    const point = await toPage(page, x, y);
    await page.mouse.move(point.x, point.y, { steps: 6 });
  }
  await page.mouse.up();
  await page.keyboard.press('t');

  const rows = page.getByTestId('roi-row');
  await expect(rows).toHaveCount(3);
  await expect(rows.nth(0)).toContainText('Ellipse');
  await expect(rows.nth(1)).toContainText('Polygon');
  await expect(rows.nth(2)).toContainText('Freehand');

  const rois = await storedRois(page);
  const counts = await expectedPixelCounts(rois);
  for (let i = 0; i < 3; i += 1) {
    await expect(rows.nth(i)).toContainText(`${counts[i].toLocaleString('en-US')} px`);
  }

  await page.keyboard.press('Shift+M');
  await expect(page.getByTestId('results-table').locator('tbody tr')).toHaveCount(15);
  const results = await resultRows(page);
  expect(results.every((r) => r.status === 'ok')).toBe(true);
  expect([...new Set(results.map((r) => r.roiId))]).toEqual(rois.map((r) => r.id));
});

test('navigates the image and edits ROIs', async ({ page }) => {
  const readout = page.getByTestId('pixel-readout');
  const box = (await page.getByTestId('image-canvas').boundingBox())!;

  /** Hovers the centre of the image pixel under a page position and checks the readout against the decoded image */
  async function expectReadoutNear(pageX: number, pageY: number): Promise<void> {
    const { scale, x, y } = await viewport(page);
    const column = Math.floor((pageX - box.x - x) / scale);
    const row = Math.floor((pageY - box.y - y) / scale);
    const centre = await toPage(page, column + 0.5, row + 0.5);
    // Whole CSS pixels: WebKit rounds mouse positions, which stays inside the pixel at 400 % and more
    await page.mouse.move(Math.round(centre.x), Math.round(centre.y));
    await expect(readout).toHaveText(new RegExp(`x ${column}\\s+y ${row}\\s+value ${lena.pixels[row * lena.width + column]}$`));
  }

  // Hover readout when zoomed in with keys, after wheel zoom and after panning
  const before = await viewport(page);
  for (let i = 0; i < 3; i += 1) {
    await page.keyboard.press('=');
  }
  expect((await viewport(page)).scale).toBeGreaterThanOrEqual(4);
  await expectReadoutNear(box.x + box.width / 2 + 13, box.y + box.height / 2 - 7);

  const beforeWheel = await viewport(page);
  await page.mouse.wheel(0, -200);
  await expect.poll(async () => (await viewport(page)).scale).toBeGreaterThan(beforeWheel.scale);
  await expectReadoutNear(box.x + box.width / 2 - 40, box.y + box.height / 2 + 25);

  const beforeArrow = await viewport(page);
  await page.keyboard.press('ArrowLeft');
  expect((await viewport(page)).x).toBeCloseTo(beforeArrow.x + 50, 6);

  const beforeSpace = await viewport(page);
  await page.mouse.move(box.x + 300, box.y + 300);
  await page.keyboard.down('Space');
  await page.mouse.down();
  await page.mouse.move(box.x + 260, box.y + 330, { steps: 5 });
  await page.mouse.up();
  await page.keyboard.up('Space');
  const afterSpace = await viewport(page);
  expect(afterSpace.x).toBeCloseTo(beforeSpace.x - 40, 0);
  expect(afterSpace.y).toBeCloseTo(beforeSpace.y + 30, 0);

  // Draw, add and frame an ROI with Z
  await page.keyboard.press('0');
  await page.keyboard.press('r');
  await drag(page, [300, 120], [360, 170]);
  await page.keyboard.press('t');
  await page.keyboard.press('z');
  const framed = await viewport(page);
  // Centre of the stored rectangle; the drawn one differs from the intended coordinates by the mouse rounding
  const [drawn] = await storedRois(page);
  const shape = drawn.shape as { x: number; y: number; width: number; height: number };
  const centre = await toPage(page, shape.x + shape.width / 2, shape.y + shape.height / 2);
  expect(centre.x).toBeCloseTo(box.x + box.width / 2, 0);
  expect(centre.y).toBeCloseTo(box.y + box.height / 2, 0);
  expect(framed.scale).toBeGreaterThan(before.scale);

  // The navigator appears when zoomed in; dragging its rectangle pans the view
  const navigatorView = page.locator('.navigator-view');
  await expect(navigatorView).toBeVisible();
  const viewBox = (await navigatorView.boundingBox())!;
  const beforeNavigator = await viewport(page);
  await page.mouse.move(viewBox.x + viewBox.width / 2, viewBox.y + viewBox.height / 2);
  await page.mouse.down();
  await page.mouse.move(viewBox.x + viewBox.width / 2 - 10, viewBox.y + viewBox.height / 2, { steps: 4 });
  await page.mouse.up();
  expect((await viewport(page)).x).toBeGreaterThan(beforeNavigator.x);

  // Hovering the ROI with the pointer tool shows its tooltip
  await page.getByRole('button', { name: /^Pointer/ }).click();
  await page.keyboard.press('z');
  const roiCentre = await toPage(page, 330, 145);
  await page.mouse.move(roiCentre.x, roiCentre.y);
  await expect(page.getByTestId('roi-tooltip')).toContainText('ROI 1');

  // Delete and undo
  await page.mouse.click(roiCentre.x, roiCentre.y);
  await page.keyboard.press('Delete');
  await expect(page.getByTestId('roi-row')).toHaveCount(0);
  await page.keyboard.press('ControlOrMeta+z');
  await expect(page.getByTestId('roi-row')).toHaveCount(1);
});
