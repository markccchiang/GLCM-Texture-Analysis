// End-to-end test of the Plot view of the Results panel: the charts draw the values of the results store

import fs from 'node:fs/promises';
import { expect, test } from '@playwright/test';
import { drawThreeRois, openSample, resultRows, storedRois } from './helpers.js';

interface SettingsHook {
  __glcm: { settings: { getState(): { settings: object; setSettings(settings: object): void } }; rois: { getState(): { hoveredId: string | null } } };
}

test('plots a feature per ROI, per direction and against the distance', async ({ page }) => {
  await openSample(page);
  await page.evaluate(() => {
    const state = (window as unknown as SettingsHook).__glcm.settings.getState();
    state.setSettings({ ...state.settings, distances: [1, 2] });
  });
  await drawThreeRois(page);
  await page.keyboard.press('Shift+M');
  await expect(page.getByTestId('results-table').locator('tbody tr')).toHaveCount(30);
  const rois = await storedRois(page);
  const rows = await resultRows(page);
  const contrast = (roiId: string, distance: number, direction: string) =>
    rows.find((row) => row.roiId === roiId && row.distance === distance && row.direction === direction)!.values.Contrast;

  const section = page.locator('section[aria-label^="Results"]');
  await section.getByText('Plot', { exact: true }).click();
  await section.getByLabel('Feature').click();
  await page.getByRole('option', { name: 'Contrast', exact: true }).click();

  const bars = section.getByTestId('plot-bar');
  await expect(bars).toHaveCount(3);
  for (const [index, roi] of rois.entries()) {
    await expect(bars.nth(index)).toHaveAttribute('data-value', String(contrast(roi.id, 1, 'mean')));
  }
  await section.getByRole('combobox', { name: 'Distance' }).click();
  await page.getByRole('option', { name: 'd = 2' }).click();
  await expect(bars.first()).toHaveAttribute('data-value', String(contrast(rois[0].id, 2, 'mean')));

  // Hovering a bar highlights its ROI, as hovering a table row does
  await bars.nth(1).hover();
  await expect.poll(() => page.evaluate(() => (window as unknown as SettingsHook).__glcm.rois.getState().hoveredId)).toBe(rois[1].id);

  await section.getByText('Box', { exact: true }).click();
  await expect(section.getByTestId('plot-box')).toHaveCount(3);

  await section.getByText('Directions', { exact: true }).click();
  const polar = section.getByTestId('plot-polar');
  await expect(polar).toHaveCount(3);
  await expect(polar.first()).toHaveAttribute('data-values', JSON.stringify(['0', '45', '90', '135'].map((direction) => contrast(rois[0].id, 2, direction))));

  await section.getByText('Distance', { exact: true }).click();
  await expect(section.getByTestId('plot-line')).toHaveCount(3);
  await expect(section.getByTestId('plot-point')).toHaveCount(6);

  const [download] = await Promise.all([page.waitForEvent('download'), section.getByRole('button', { name: 'Save SVG' }).click()]);
  expect(download.suggestedFilename()).toBe('camera-Contrast-distance.svg');
  const svg = await fs.readFile((await download.path())!, 'utf8');
  expect(svg).toContain('<svg');
  expect(svg).toContain(rois[0].name);
  expect(svg).not.toContain('var(');

  await section.getByText('Table', { exact: true }).click();
  await expect(page.getByTestId('results-table')).toBeVisible();
});
