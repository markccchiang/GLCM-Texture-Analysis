// End-to-end test of the ruler tool: the readout follows the drawn line, gives millimetres with a pixel spacing, and the
// ruler disappears with Escape and when another tool is chosen

import { expect, test, type Page } from '@playwright/test';
import { drag, openSample } from './helpers.js';

interface RulerHooks {
  __glcm: {
    viewer: {
      getState(): {
        ruler: { start: { x: number; y: number }; end: { x: number; y: number } } | null;
        setPixelSpacing(spacing: { x: number; y: number } | null): void;
      };
    };
  };
}

const storedRuler = (page: Page) => page.evaluate(() => (window as unknown as RulerHooks).__glcm.viewer.getState().ruler);

test('measures a distance with the ruler and removes it', async ({ page }) => {
  await openSample(page);
  await page.keyboard.press('l');
  await drag(page, [100, 120], [220, 280]);

  // WebKit rounds pointer positions, so the expected text comes from the stored line
  const line = (await storedRuler(page))!;
  expect(line).not.toBeNull();
  const dx = line.end.x - line.start.x;
  const dy = line.end.y - line.start.y;
  const length = Math.hypot(dx, dy);
  const readout = page.getByTestId('ruler-readout');
  await expect(readout).toHaveText(`ruler ${length.toFixed(1)} px · ${((Math.atan2(-dy, dx) * 180) / Math.PI).toFixed(1)}°`);

  await page.evaluate(() => (window as unknown as RulerHooks).__glcm.viewer.getState().setPixelSpacing({ x: 0.5, y: 0.5 }));
  await expect(readout).toContainText(`${Number((length * 0.5).toPrecision(4))} mm`);

  await page.keyboard.press('Escape');
  await expect(readout).toHaveCount(0);

  await drag(page, [50, 50], [90, 50]);
  await expect(readout).toContainText('px');
  await page.getByRole('button', { name: /^Pointer/ }).click();
  await expect(readout).toHaveCount(0);
});
