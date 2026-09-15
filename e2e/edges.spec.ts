// End-to-end test of the edge map and the livewire tool: the PNG the app shows and the outline it creates equal the
// addon's results for the same image and settings

import path from 'node:path';
import * as native from '@glcm/native';
import { expect, test, type Page } from '@playwright/test';
import { chooseMenuItem, openSample, ROOT, toPage, viewport } from './helpers.js';

interface LivewireHooks {
  __glcm: { rois: { getState(): { activeShape: { type: string; points?: Array<[number, number]> } | null } } };
}

const camera = () => native.decodeImageFile(path.join(ROOT, 'samples', 'textures', 'camera.png'));

const activeShape = (page: Page) => page.evaluate(() => (window as unknown as LivewireHooks).__glcm.rois.getState().activeShape);

test('the edge map shows the core rendering with the chosen thresholds', async ({ page }) => {
  await openSample(page);
  // Automatic limits such as low=10.44 must not match low=10, so the parameters are compared exactly
  const edgeResponse = (low: string, high: string) =>
    page.waitForResponse((response) => {
      const url = new URL(response.url());
      return url.pathname.endsWith('/edges.png') && url.searchParams.get('low') === low && url.searchParams.get('high') === high;
    });

  await chooseMenuItem(page, 'View', 'Show Edge Map');
  const card = page.getByRole('region', { name: 'Edge map' });
  await expect(card).toBeVisible();
  await expect(card).toContainText('95th percentile');

  const response = edgeResponse('10', '30');
  await card.getByRole('textbox', { name: 'High threshold' }).fill('30');
  await card.getByRole('textbox', { name: 'Low threshold' }).fill('10');
  // The body Playwright records for a response the app streamed into a blob can be incomplete, so the same URL is fetched
  // again for the comparison
  const url = (await response).url();
  expect(new URL(url).searchParams.get('method')).toBe('canny');
  const png = await (await page.request.get(url)).body();

  const image = await camera();
  const expected = await native.renderEdgeMap(image.pixels, image.width, image.height, image.bitDepth, 'canny', 1, 10, 30, 4096);
  expect(png.equals(expected)).toBe(true);

  await card.getByRole('button', { name: 'Hide edge map' }).click();
  await expect(card).toHaveCount(0);
});

test('the livewire outline follows the paths between the clicked points', async ({ page }) => {
  await openSample(page);
  await page.getByTestId('image-canvas').click({ position: { x: 5, y: 5 } });
  await page.keyboard.press('i');
  await expect(page.getByRole('button', { name: /^Livewire \(I\)/ })).toHaveAttribute('aria-pressed', 'true');

  // Three points around the cameraman's head. WebKit rounds mouse positions to whole CSS pixels, so the clicks are sent at
  // rounded positions and the clicked pixels are worked out from those positions.
  const box = (await page.getByTestId('image-canvas').boundingBox())!;
  const { scale, x: offsetX, y: offsetY } = await viewport(page);
  const anchors: Array<[number, number]> = [];
  for (const [x, y] of [
    [200, 70],
    [290, 110],
    [220, 190],
  ]) {
    const target = await toPage(page, x + 0.5, y + 0.5);
    const [pageX, pageY] = [Math.round(target.x), Math.round(target.y)];
    anchors.push([Math.floor((pageX - box.x - offsetX) / scale), Math.floor((pageY - box.y - offsetY) / scale)]);
    await page.mouse.click(pageX, pageY);
  }
  await page.keyboard.press('Enter');

  const image = await camera();
  const segments = await Promise.all(
    anchors.map(([x, y], i) => {
      const [toX, toY] = anchors[(i + 1) % anchors.length];
      return native.livewirePath(image.pixels, image.width, image.height, image.bitDepth, x, y, toX, toY, 1);
    }),
  );
  // Joined as the app joins them: shared end points once, the closing point dropped
  const joined: Array<[number, number]> = [];
  for (const segment of segments) {
    for (const point of segment) {
      const last = joined.at(-1);
      if (!last || last[0] !== point[0] || last[1] !== point[1]) {
        joined.push(point);
      }
    }
  }
  joined.pop();
  await expect.poll(() => activeShape(page)).toEqual({ type: 'polygon', points: joined });
});
