// End-to-end test of the colour tables: the displayed pixels are the table's colours for the window/level display value,
// with the WebGL2 renderer and with the lookup-table renderer

import { expect, test, type Page } from '@playwright/test';
import { openSample } from './helpers.js';

interface ViewerHooks {
  __glcm: {
    viewer: {
      getState(): {
        displaySource: unknown;
        image: { info: { width: number }; raw: { samples: ArrayLike<number> } | null } | null;
        setWindow(min: number, max: number): void;
      };
    };
  };
}

/** RGB of an image pixel as displayed */
const displayed = (page: Page, x: number, y: number) =>
  page.evaluate(
    ([px, py]) => {
      const source = (window as unknown as ViewerHooks).__glcm.viewer.getState().displaySource;
      if (!(source instanceof HTMLCanvasElement)) {
        return null;
      }
      const copy = document.createElement('canvas');
      copy.width = source.width;
      copy.height = source.height;
      const context = copy.getContext('2d')!;
      context.drawImage(source, 0, 0);
      return Array.from(context.getImageData(px, py, 1, 1).data.subarray(0, 3));
    },
    [x, y],
  );

const setWindow = (page: Page, min: number, max: number) =>
  page.evaluate(([low, high]) => (window as unknown as ViewerHooks).__glcm.viewer.getState().setWindow(low, high), [min, max]);

for (const renderer of ['WebGL2', 'Lookup table'] as const) {
  test(`colours the image with the chosen table (${renderer})`, async ({ page }) => {
    if (renderer === 'Lookup table') {
      await page.addInitScript(() => window.localStorage.setItem('glcm.preferences', JSON.stringify({ state: { useWebGl: false }, version: 1 })));
    }
    await openSample(page);
    const status = page.getByTestId('status-bar');
    if (renderer === 'WebGL2') {
      test.skip(!(await status.textContent())?.includes('WebGL2'), 'This browser does not render with WebGL2');
    } else {
      await expect(status).toContainText('Lookup table');
    }

    const x = 100;
    const y = 100;
    const value = await page.evaluate(
      ([px, py]) => {
        const image = (window as unknown as ViewerHooks).__glcm.viewer.getState().image!;
        return image.raw!.samples[py * image.info.width + px];
      },
      [x, y],
    );
    expect(value).toBeLessThan(255);

    await setWindow(page, 0, 255);
    await expect.poll(() => displayed(page, x, y)).toEqual([value, value, value]);

    await page.getByRole('button', { name: 'Window/level settings' }).click();
    await page.getByRole('radio', { name: 'Inverted' }).click();
    await expect(page.getByRole('radio', { name: 'Inverted' })).toHaveAttribute('aria-checked', 'true');
    await expect.poll(() => displayed(page, x, y)).toEqual([255 - value, 255 - value, 255 - value]);

    // Viridis at both ends: a window above the value shows display value 0, a window at the value 255
    await page.getByRole('radio', { name: 'Viridis' }).click();
    await setWindow(page, value + 1, value + 1);
    await expect.poll(() => displayed(page, x, y)).toEqual([68, 1, 84]);
    await setWindow(page, value, value);
    await expect.poll(() => displayed(page, x, y)).toEqual([253, 231, 37]);

    await page.getByRole('radio', { name: 'Gray' }).click();
    await setWindow(page, 0, 255);
    await expect.poll(() => displayed(page, x, y)).toEqual([value, value, value]);
  });
}
