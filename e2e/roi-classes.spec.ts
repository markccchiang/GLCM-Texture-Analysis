// End-to-end test of ROI classes: classes assigned in the ROI Manager and with ⇧ and the number keys travel into the
// measurement, the results table, the exported CSV and the grouped plot

import fs from 'node:fs/promises';
import type { RoiShape } from '@glcm/api';
import { expect, test, type Page } from '@playwright/test';
import { chooseMenuItem, openSample, parseCsv } from './helpers.js';

interface ClassHooks {
  __glcm: {
    rois: {
      getState(): {
        rois: Array<{ id: string; name: string; color: string; className?: string }>;
        importRois(rois: Array<{ name: string; color: string; shape: RoiShape }>): string[];
        select(ids: string[]): void;
      };
    };
  };
}

const classOf = (page: Page) =>
  page.evaluate(() => (window as unknown as ClassHooks).__glcm.rois.getState().rois.map(({ name, color, className }) => ({ name, color, className: className ?? null })));

const select = (page: Page, indexes: number[]) =>
  page.evaluate((chosen) => {
    const rois = (window as unknown as ClassHooks).__glcm.rois.getState();
    rois.select(chosen.map((index) => rois.rois[index].id));
  }, indexes);

test('classes go into the results table, the CSV export and the grouped plot', async ({ page }) => {
  await openSample(page);
  await page.evaluate(() => {
    const rois = (window as unknown as ClassHooks).__glcm.rois.getState();
    rois.importRois([
      { name: 'Coat', color: '', shape: { type: 'rectangle', x: 110, y: 300, width: 40, height: 40 } },
      { name: 'Grass', color: '', shape: { type: 'rectangle', x: 380, y: 400, width: 40, height: 40 } },
      { name: 'Sky', color: '', shape: { type: 'rectangle', x: 40, y: 40, width: 40, height: 40 } },
    ]);
  });

  await chooseMenuItem(page, 'ROI', 'ROI Classes…');
  const dialog = page.getByRole('dialog', { name: 'ROI Classes' });
  await dialog.getByRole('button', { name: 'Add class' }).click();
  await dialog.getByRole('button', { name: 'Add class' }).click();
  await dialog.getByRole('textbox', { name: 'Name of class 1' }).fill('dark');
  await dialog.getByRole('textbox', { name: 'Name of class 2' }).fill('bright');
  await dialog.getByRole('textbox', { name: 'Name of class 2' }).press('Enter');
  await dialog.getByRole('group', { name: 'Colour of bright' }).getByRole('button', { name: 'Colour #00C2FF' }).click();
  await dialog.getByRole('button', { name: 'Done' }).click();
  await expect(page.getByRole('dialog')).toHaveCount(0);

  // ⇧1 gives the Coat the first class; ⇧2 the others the second
  await page.getByTestId('image-canvas').click({ position: { x: 5, y: 5 } });
  await select(page, [0]);
  await page.keyboard.press('Shift+Digit1');
  await select(page, [1, 2]);
  await page.keyboard.press('Shift+Digit2');
  await expect.poll(() => classOf(page)).toEqual([
    { name: 'Coat', color: expect.any(String), className: 'dark' },
    { name: 'Grass', color: '#00C2FF', className: 'bright' },
    { name: 'Sky', color: '#00C2FF', className: 'bright' },
  ]);
  await expect(page.getByTestId('roi-class-tag')).toHaveCount(3);

  await page.keyboard.press('Shift+M');
  const table = page.getByTestId('results-table');
  await expect(table.getByRole('columnheader', { name: 'Class' })).toBeVisible({ timeout: 30_000 });
  await expect(table.getByRole('cell', { name: 'dark', exact: true }).first()).toBeVisible();

  await page.getByRole('combobox', { name: 'Class filter' }).click();
  await page.getByRole('option', { name: 'bright', exact: true }).click();
  await expect(table.getByRole('cell', { name: 'dark', exact: true })).toHaveCount(0);
  await expect(table.getByRole('cell', { name: 'bright', exact: true }).first()).toBeVisible();

  const download = page.waitForEvent('download');
  await chooseMenuItem(page, 'File', /^Export Results as CSV/);
  const { header, rows } = parseCsv(await fs.readFile(await (await download).path(), 'utf8'));
  const classColumn = header.indexOf('roiClass');
  expect(classColumn).toBe(header.indexOf('roiId') + 1);
  const nameColumn = header.indexOf('roiName');
  expect(new Set(rows.map((row) => `${row[nameColumn]}=${row[classColumn]}`))).toEqual(new Set(['Coat=dark', 'Grass=bright', 'Sky=bright']));

  await page.getByTestId('results-table').locator('xpath=ancestor::section').getByText('Plot', { exact: true }).click();
  await page.getByRole('switch', { name: 'Group by class' }).check();
  await expect(page.getByTestId('plot-bar')).toHaveCount(2);
});
