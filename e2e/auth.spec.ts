// Server mode (doc/ui-design-plan.md, section 8.2): the web app asks for the access token once per tab and sends it with
// uploads, raw pixels, progress events and results. Runs against the server with GLCM_API_TOKEN (playwright.config.ts).

import { expect, test } from '@playwright/test';
import { drag, resultRows, waitForImage } from './helpers.js';
import { E2E_API_TOKEN } from './token.js';

test('asks for the access token once per tab and sends it with every request', async ({ page, browser, baseURL }) => {
  await page.goto('/?testHooks');
  const prompt = page.getByRole('dialog', { name: 'Access token' });
  await expect(prompt).toBeVisible();

  await page.getByRole('textbox', { name: 'Access token' }).fill('not-the-token');
  await page.getByRole('button', { name: 'Continue' }).click();
  await expect(prompt).toContainText('did not accept');

  await page.getByRole('textbox', { name: 'Access token' }).fill(E2E_API_TOKEN);
  await page.getByRole('button', { name: 'Continue' }).click();
  await expect(prompt).toBeHidden();

  // Samples, upload, raw pixels, ROI statistics, progress events and results all need the token
  await page.getByRole('button', { name: 'Open sample image' }).click();
  await waitForImage(page);
  await expect(page.getByTestId('status-bar')).toContainText(/WebGL2|Lookup table/);
  await page.keyboard.press('r');
  await drag(page, [100, 100], [160, 150]);
  await page.keyboard.press('t');
  await expect(page.getByTestId('roi-row')).toContainText(/\d px/);
  await page.keyboard.press('m');
  await expect(page.getByTestId('results-table').locator('tbody tr')).toHaveCount(5);
  expect((await resultRows(page)).every((row) => row.status === 'ok')).toBe(true);

  // The token lasts for the tab: no prompt after a reload
  await page.reload();
  await expect(page.getByRole('button', { name: 'Open sample image' })).toBeVisible();
  await expect(prompt).toBeHidden();

  // Another session asks again, and requests without the token are rejected
  const other = await browser.newContext({ baseURL });
  const otherPage = await other.newPage();
  await otherPage.goto('/');
  await expect(otherPage.getByRole('dialog', { name: 'Access token' })).toBeVisible();
  expect((await otherPage.request.get('/api/v1/catalog')).status()).toBe(401);
  expect((await otherPage.request.get('/api/v1/health')).status()).toBe(200);
  await other.close();
});
