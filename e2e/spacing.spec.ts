// End-to-end test of pixel spacing: entered in Image Info, it adds a scale bar and ROI areas in mm², travels with the
// measurement into the exported CSV, and is remembered for the image

import fs from 'node:fs/promises';
import { expect, test, type Page } from '@playwright/test';
import { chooseMenuItem, drag, openSample, parseCsv, storedRois } from './helpers.js';

/** Waits for the closing transition too: until then the overlay would catch the next pointer events on the canvas */
async function closeDialog(page: Page): Promise<void> {
  await page.keyboard.press('Escape');
  await expect(page.locator('.mantine-Modal-overlay')).toHaveCount(0);
}

async function setSpacing(page: Page, width: string, height: string): Promise<void> {
  await chooseMenuItem(page, 'Image', 'Image Info');
  const dialog = page.getByRole('dialog', { name: 'Image Info' });
  await dialog.getByLabel('Pixel width (mm)').fill(width);
  await dialog.getByLabel('Pixel height (mm)').fill(height);
  await dialog.getByRole('button', { name: 'Apply' }).click();
  await expect(dialog.getByTestId('pixel-spacing-status')).toContainText('(entered)');
  await closeDialog(page);
}

test('uses an entered pixel spacing for the scale bar, ROI areas and exported results', async ({ page }) => {
  await openSample(page);
  // camera.png has no resolution in its file
  await expect(page.getByTestId('scale-bar')).toHaveCount(0);

  await setSpacing(page, '0.5', '0.25');
  await expect(page.getByTestId('scale-bar')).toBeVisible();
  await expect(page.getByTestId('scale-bar')).toContainText('horizontally');
  await expect(page.getByTestId('anisotropy-note')).toContainText('0.5 × 0.25 mm');

  await page.keyboard.press('r');
  await drag(page, [120, 90], [190, 150]);
  await page.keyboard.press('t');
  const row = page.getByTestId('roi-row').first();
  await expect(row).toContainText(' px');
  const pixels = Number((await row.locator('.roi-pixels').innerText()).match(/([\d,]+) px/)![1].replace(/,/g, ''));
  await expect(row).toContainText(`${Number((pixels * 0.5 * 0.25).toPrecision(3))} mm²`);

  await page.keyboard.press('m');
  const table = page.getByTestId('results-table');
  await expect(table.locator('tbody tr')).toHaveCount(5);
  await expect(table.getByRole('columnheader', { name: 'Area (mm²)' })).toBeVisible();

  const [download] = await Promise.all([page.waitForEvent('download'), chooseMenuItem(page, 'File', 'Export Results as CSV')]);
  const { comments, header, rows } = parseCsv(await fs.readFile((await download.path())!, 'utf8'));
  expect(comments).toContain('# pixelSpacingMm=0.5;0.25');
  const [roi] = await storedRois(page);
  expect(header.indexOf('areaMm2')).toBe(header.indexOf('pixelCount') + 1);
  for (const csvRow of rows) {
    expect(csvRow[header.indexOf('roiId')]).toBe(roi.id);
    expect(Number(csvRow[header.indexOf('areaMm2')])).toBe(Number(csvRow[header.indexOf('pixelCount')]) * 0.5 * 0.25);
  }

  // Opening the image again restores the spacing chosen for it; Clear removes it
  await openSample(page);
  await expect(page.getByTestId('scale-bar')).toBeVisible();
  await chooseMenuItem(page, 'Image', 'Image Info');
  const dialog = page.getByRole('dialog', { name: 'Image Info' });
  await expect(dialog.getByTestId('pixel-spacing-status')).toContainText('0.5 × 0.25 mm per pixel (entered)');
  await dialog.getByRole('button', { name: 'Clear' }).click();
  await expect(dialog.getByTestId('pixel-spacing-status')).toContainText('No pixel spacing');
  await closeDialog(page);
  await expect(page.getByTestId('scale-bar')).toHaveCount(0);
  await expect(page.getByTestId('anisotropy-note')).toHaveCount(0);
});
