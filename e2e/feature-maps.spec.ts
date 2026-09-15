// End-to-end test of feature maps: the map shown and saved equals the addon's map of the same image and settings

import fs from 'node:fs/promises';
import path from 'node:path';
import type { AnalysisSettings } from '@glcm/api';
import * as native from '@glcm/native';
import { expect, test, type Page } from '@playwright/test';
import { PNG } from 'pngjs';
import { chooseMenuItem, openSample, ROOT, storedSettings } from './helpers.js';

interface MapHooks {
  __glcm: {
    featureMap: { getState(): { map: { info: { status: string }; values: Float32Array | null } | null } };
  };
}

const mapValues = (page: Page) =>
  page.evaluate(() => {
    const values = (window as unknown as MapHooks).__glcm.featureMap.getState().map?.values;
    return values ? Array.from(values) : null;
  });

/** Whether the centre of some canvas of the stage shows a colour that is not gray (the overlay over a gray image) */
const centreIsColoured = (page: Page) =>
  page.evaluate(() =>
    [...document.querySelectorAll<HTMLCanvasElement>('.canvas-stage canvas')].some((canvas) => {
      const [r, g, b, a] = canvas.getContext('2d')!.getImageData(Math.floor(canvas.width / 2), Math.floor(canvas.height / 2), 1, 1).data;
      return a > 0 && Math.max(Math.abs(r - g), Math.abs(g - b), Math.abs(r - b)) > 20;
    }),
  );

test('computes a feature map, shows it over the image and saves it', async ({ page }) => {
  await openSample(page);
  const analysis = (await storedSettings(page)) as AnalysisSettings;

  await chooseMenuItem(page, 'Analyze', 'Feature Map…');
  const dialog = page.getByRole('dialog', { name: 'Feature Map' });
  await expect(dialog.getByTestId('feature-map-grid')).toContainText('512 × 512 points, one window every 1 px');
  await dialog.getByRole('textbox', { name: 'Window' }).fill('7');
  await dialog.getByText('Manual', { exact: true }).click();
  await dialog.getByRole('textbox', { name: 'Step in pixels' }).fill('8');
  await expect(dialog.getByTestId('feature-map-grid')).toContainText('64 × 64 points, one window every 8 px');
  await dialog.getByRole('button', { name: 'Compute' }).click();
  await expect(page.getByRole('dialog')).toHaveCount(0);

  const card = page.getByRole('region', { name: 'Feature map' });
  await expect(card.getByRole('button', { name: 'Save TIFF' })).toBeVisible({ timeout: 60_000 });
  await expect(card).toContainText('64×64 · window 7 px · step 8');

  const camera = await native.decodeImageFile(path.join(ROOT, 'samples', 'textures', 'camera.png'));
  const settings = {
    feature: 'Contrast',
    window: 7,
    step: 8,
    grayLevels: analysis.grayLevels,
    quantization: analysis.quantization,
    distance: Math.min(...analysis.distances),
    directions: [...analysis.directions].sort((a, b) => a - b),
    logBase: analysis.logBase,
  };
  const expected = Array.from(await native.computeFeatureMap(camera.pixels, camera.width, camera.height, camera.bitDepth, JSON.stringify(settings), 0, 64));
  expect(await mapValues(page)).toEqual(expected);

  await expect.poll(() => centreIsColoured(page)).toBe(true);
  await page.getByTestId('image-canvas').hover();
  await expect(card.getByTestId('feature-map-value')).toContainText(/\(\d+, \d+\): /);

  const tiffDownload = page.waitForEvent('download');
  await card.getByRole('button', { name: 'Save TIFF' }).click();
  const tiff = await tiffDownload;
  expect(tiff.suggestedFilename()).toBe('camera-Contrast-map.tif');
  const bytes = await fs.readFile(await tiff.path());
  const samples = bytes.subarray(bytes.length - 64 * 64 * 4);
  const view = new DataView(samples.buffer, samples.byteOffset, samples.byteLength);
  expect(Array.from({ length: 64 * 64 }, (_, i) => view.getFloat32(i * 4, true))).toEqual(expected);

  const pngDownload = page.waitForEvent('download');
  await card.getByRole('button', { name: 'Save PNG' }).click();
  const png = await pngDownload;
  expect(png.suggestedFilename()).toBe('camera-Contrast-map.png');
  const decoded = PNG.sync.read(await fs.readFile(await png.path()));
  expect([decoded.width, decoded.height]).toEqual([64, 64]);

  await card.getByRole('switch', { name: 'Show over the image' }).uncheck();
  await expect.poll(() => centreIsColoured(page)).toBe(false);

  await card.getByRole('button', { name: 'Close feature map' }).click();
  await expect(card).toHaveCount(0);
});

test('offers the co-occurrence features that can be mapped', async ({ page }) => {
  await openSample(page);
  await chooseMenuItem(page, 'Analyze', 'Feature Map…');
  const dialog = page.getByRole('dialog', { name: 'Feature Map' });
  await dialog.getByRole('combobox', { name: 'Feature' }).click();
  await expect(page.getByRole('option', { name: 'Entropy', exact: true })).toBeVisible();
  await expect(page.getByRole('option', { name: /Cluster Shade/ })).toHaveCount(1);
  await expect(page.getByRole('option', { name: /Maximal Correlation/ })).toHaveCount(0);
  await expect(page.getByRole('option', { name: /Run Entropy|Skewness/ })).toHaveCount(0);

  await dialog.getByRole('textbox', { name: 'Window' }).fill('4');
  await expect(dialog.getByTestId('feature-map-grid')).toContainText('odd number of pixels');
  await expect(dialog.getByRole('button', { name: 'Compute' })).toBeDisabled();
});
