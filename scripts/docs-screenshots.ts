// Captures the UI screenshots of the user guide into doc/user/images.
//
//   npm run build:web && npm run docs:screenshots
//
// Starts two temporary servers (without and with an access token), opens the sample image in Chromium, draws and
// measures ROIs, and saves the screenshots. Run it again after UI changes so the guide stays accurate.

import { spawn, type ChildProcess } from 'node:child_process';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { chromium, expect, type Locator, type Page } from '@playwright/test';
import { chooseMenuItem, clickAt, drag, toPage, waitForImage } from '../e2e/helpers.js';
import { E2E_API_TOKEN } from '../e2e/token.js';

const ROOT = path.resolve(import.meta.dirname, '..');
const OUTPUT = path.join(ROOT, 'doc', 'user', 'images');
const PORT = 8190;
const TOKEN_PORT = 8191;
const VIEWPORT = { width: 1440, height: 960 };

const SETTINGS = {
  features: ['Mean', 'Energy', 'Contrast', 'HomogeneityII', 'CorrelationII', 'Entropy'],
  grayLevels: 32,
  quantization: { method: 'fixedRange', min: 0, max: 255, binWidth: 8 },
  distances: [1],
  directions: [0, 45, 90, 135],
  aggregation: 'perDirectionAndMean',
  logBase: 'natural',
  score: { enabled: true, age: 45, coefficients: [1.138, -1.814, 1.416, 1.714], profile: 'calibration', intensityMin: 0, intensityMax: 255 },
};

interface Server {
  process: ChildProcess;
  dataDir: string;
}

async function startServer(port: number, apiToken: string): Promise<Server> {
  const dataDir = await fs.mkdtemp(path.join(os.tmpdir(), 'glcm-docs-'));
  const child = spawn(process.execPath, ['--import', 'tsx', 'server/src/main.ts'], {
    cwd: ROOT,
    env: { ...process.env, GLCM_HOST: '127.0.0.1', GLCM_PORT: String(port), GLCM_DATA_DIR: dataDir, GLCM_API_TOKEN: apiToken, GLCM_LOG_LEVEL: 'warn', UV_THREADPOOL_SIZE: '8' },
    stdio: ['ignore', 'inherit', 'inherit'],
  });
  for (let attempt = 0; attempt < 120; attempt += 1) {
    try {
      if ((await fetch(`http://127.0.0.1:${port}/api/v1/health`)).ok) {
        return { process: child, dataDir };
      }
    } catch {
      // Not listening yet
    }
    await new Promise((resolve) => setTimeout(resolve, 500));
  }
  child.kill();
  throw new Error(`The server on port ${port} did not start`);
}

async function stopServer(server: Server): Promise<void> {
  server.process.kill('SIGTERM');
  await new Promise((resolve) => server.process.once('exit', resolve));
  await fs.rm(server.dataDir, { recursive: true, force: true });
}

type Clip = { x: number; y: number; width: number; height: number };

async function shot(page: Page, name: string, target?: Locator | Clip, margin = 0): Promise<void> {
  let clip: Clip | undefined;
  if (target && 'boundingBox' in target) {
    const box = (await target.boundingBox())!;
    clip = { x: box.x - margin, y: box.y - margin, width: box.width + 2 * margin, height: box.height + 2 * margin };
  } else {
    clip = target;
  }
  if (clip) {
    const viewport = page.viewportSize() ?? VIEWPORT;
    const x = Math.max(0, Math.floor(clip.x));
    const y = Math.max(0, Math.floor(clip.y));
    clip = { x, y, width: Math.min(viewport.width - x, Math.ceil(clip.width)), height: Math.min(viewport.height - y, Math.ceil(clip.height)) };
  }
  await page.screenshot({ path: path.join(OUTPUT, `${name}.png`), clip, animations: 'disabled' });
  console.log(`✓ ${name}.png`);
}

/** A dialog after its opening transition, so that its final position is captured */
async function dialogShot(page: Page, name: string, dialog: Locator): Promise<void> {
  await dialog.waitFor();
  await page.waitForTimeout(600);
  await shot(page, name, dialog);
}

/** Numbered markers placed relative to elements, for the main window figure */
async function addCallouts(page: Page, callouts: Array<{ selector: string; label: string; dx: number; dy: number }>): Promise<void> {
  await page.evaluate((items) => {
    for (const { selector, label, dx, dy } of items) {
      const element = document.querySelector(selector);
      if (!element) {
        throw new Error(`No element for ${selector}`);
      }
      const box = element.getBoundingClientRect();
      const marker = document.createElement('div');
      marker.className = 'docs-callout';
      marker.textContent = label;
      Object.assign(marker.style, {
        position: 'fixed',
        left: `${box.left + dx}px`,
        top: `${box.top + dy}px`,
        width: '26px',
        height: '26px',
        borderRadius: '50%',
        background: '#fa5252',
        color: '#ffffff',
        font: '700 14px/26px system-ui, sans-serif',
        textAlign: 'center',
        boxShadow: '0 0 0 2px #ffffff, 0 2px 8px rgba(0, 0, 0, 0.6)',
        zIndex: '10000',
        pointerEvents: 'none',
      });
      document.body.append(marker);
    }
  }, callouts);
}

async function removeCallouts(page: Page): Promise<void> {
  await page.evaluate(() => document.querySelectorAll('.docs-callout').forEach((marker) => marker.remove()));
}

/** Toasts would cover parts of the screenshots */
async function hideNotifications(page: Page): Promise<void> {
  await page.addStyleTag({ content: '.mantine-Notifications-root { display: none !important; }' });
}

async function main(): Promise<void> {
  await fs.mkdir(OUTPUT, { recursive: true });
  const server = await startServer(PORT, '');
  const tokenServer = await startServer(TOKEN_PORT, E2E_API_TOKEN);
  const browser = await chromium.launch();

  try {
    const context = await browser.newContext({ baseURL: `http://127.0.0.1:${PORT}`, viewport: VIEWPORT, locale: 'en-US', colorScheme: 'dark' });
    await context.addInitScript((settings) => {
      window.localStorage.setItem('glcm.analysisSettings', JSON.stringify({ state: { settings }, version: 1 }));
    }, SETTINGS);
    const page = await context.newPage();

    // Start screen
    await page.goto('/?testHooks');
    await hideNotifications(page);
    await page.getByRole('button', { name: 'Open sample image' }).waitFor();
    await shot(page, 'start-screen');

    // Image with four ROIs, measured
    await page.getByRole('button', { name: 'Open sample image' }).click();
    await waitForImage(page);
    await page.keyboard.press('r');
    await drag(page, [40, 30], [140, 95]);
    await page.keyboard.press('t');
    await page.keyboard.press('e');
    await drag(page, [45, 280], [125, 400]);
    await page.keyboard.press('t');
    await page.keyboard.press('p');
    for (const [x, y] of [
      [385, 270],
      [480, 262],
      [498, 370],
      [455, 430],
      [398, 375],
    ]) {
      await clickAt(page, x, y);
    }
    await page.keyboard.press('Enter');
    await page.keyboard.press('t');
    await page.keyboard.press('f');
    const outline = [
      [168, 112],
      [178, 82],
      [210, 64],
      [250, 70],
      [268, 96],
      [238, 104],
      [200, 116],
      [172, 116],
    ];
    const start = await toPage(page, outline[0][0], outline[0][1]);
    await page.mouse.move(start.x, start.y);
    await page.mouse.down();
    for (const [x, y] of outline.slice(1)) {
      const point = await toPage(page, x, y);
      await page.mouse.move(point.x, point.y, { steps: 8 });
    }
    await page.mouse.up();
    await page.keyboard.press('t');
    await page.evaluate(() => {
      const rois = (window as unknown as { __glcm: { rois: { getState(): { rois: Array<{ id: string }>; renameRoi(id: string, name: string): void; select(ids: string[]): void } } } }).__glcm.rois.getState();
      ['Sky', 'Coat', 'Grass', 'Hair'].forEach((name, i) => rois.renameRoi(rois.rois[i].id, name));
      rois.select([]);
    });
    await page.keyboard.press('Shift+M');
    await expect(page.getByTestId('results-table').locator('tbody tr')).toHaveCount(20, { timeout: 30_000 });
    await chooseMenuItem(page, 'View', 'Show ROI Labels');
    await page.getByRole('button', { name: /^Pointer/ }).click();
    await page.getByTestId('roi-row').filter({ hasText: 'Coat' }).click();
    const coatPoint = await toPage(page, 300, 180);
    await page.mouse.move(coatPoint.x, coatPoint.y);
    await page.waitForTimeout(400);

    // Without markers, for the README
    await shot(page, 'app-window');

    await addCallouts(page, [
      { selector: 'nav[aria-label="Main menu"]', label: '1', dx: 620, dy: 4 },
      { selector: '[role="toolbar"]', label: '2', dx: 900, dy: 8 },
      { selector: '[data-testid="image-canvas"]', label: '3', dx: 12, dy: 12 },
      { selector: 'section[aria-label^="ROI Manager"]', label: '4', dx: 200, dy: 1 },
      { selector: 'section[aria-label="Analysis Settings"]', label: '5', dx: 200, dy: 1 },
      { selector: 'section[aria-label^="Results"]', label: '6', dx: 230, dy: 1 },
      { selector: '[data-testid="status-bar"]', label: '7', dx: 1000, dy: 0 },
    ]);
    await shot(page, 'main-window');
    await removeCallouts(page);

    await shot(page, 'canvas-rois', page.getByTestId('image-canvas'));
    await shot(page, 'roi-manager', page.locator('section[aria-label^="ROI Manager"]'));
    // A taller window shows the whole settings panel, and a taller Results panel shows more rows
    await page.setViewportSize({ width: VIEWPORT.width, height: 1400 });
    await page.waitForTimeout(400);
    await shot(page, 'analysis-settings', page.locator('section[aria-label="Analysis Settings"]'));
    const resultsBorder = (await page.locator('.separator-horizontal').last().boundingBox())!;
    await page.mouse.move(resultsBorder.x + resultsBorder.width / 2, resultsBorder.y + resultsBorder.height / 2);
    await page.mouse.down();
    await page.mouse.move(resultsBorder.x + resultsBorder.width / 2, resultsBorder.y - 260, { steps: 10 });
    await page.mouse.up();
    await page.mouse.move(5, 1395);
    await page.waitForTimeout(400);
    await shot(page, 'results-table', page.locator('section[aria-label^="Results"]'));

    // Feature picker, which needs the taller window too
    await page.getByTestId('feature-picker-button').click();
    await page.waitForTimeout(300);
    await shot(page, 'feature-picker', page.getByRole('dialog', { name: 'Features' }));
    await page.keyboard.press('Escape');
    await page.getByRole('dialog', { name: 'Features' }).waitFor({ state: 'hidden' });

    await chooseMenuItem(page, 'View', 'Reset Layout');
    await page.setViewportSize(VIEWPORT);
    await page.waitForTimeout(400);

    // Window/level
    await page.getByRole('button', { name: 'Window/level settings' }).click();
    // The Select menus of the settings panel keep hidden dropdowns of the same class
    const popover = page.locator('.mantine-Popover-dropdown').filter({ hasText: 'Histogram' });
    await popover.waitFor();
    const toolbarBox = (await page.getByRole('toolbar').boundingBox())!;
    const popoverBox = (await popover.boundingBox())!;
    await shot(page, 'window-level', {
      x: popoverBox.x - 330,
      y: toolbarBox.y,
      width: popoverBox.width + 350,
      height: popoverBox.y + popoverBox.height - toolbarBox.y + 12,
    });
    // The popover stays open on Escape; its button toggles it
    await page.getByRole('button', { name: 'Window/level settings' }).click();
    await popover.waitFor({ state: 'hidden' });

    // Zoomed in on the selected ROI, with the navigator and an ROI tooltip
    await page.mouse.move(5, VIEWPORT.height - 5);
    await page.keyboard.press('z');
    const hover = await toPage(page, 100, 330);
    await page.mouse.move(hover.x, hover.y);
    await page.waitForTimeout(700);
    await shot(page, 'zoom-navigator', page.getByTestId('image-canvas'));
    await page.mouse.move(5, VIEWPORT.height - 5);
    await page.keyboard.press('0');

    // File menu and dialogs
    await page.getByRole('navigation', { name: 'Main menu' }).getByRole('button', { name: 'File', exact: true }).click();
    const menu = page.getByRole('menu');
    await menu.waitFor();
    const menuBox = (await menu.boundingBox())!;
    await shot(page, 'file-menu', { x: 0, y: 0, width: menuBox.x + menuBox.width + 24, height: menuBox.y + menuBox.height + 12 });
    await page.keyboard.press('Escape');

    await chooseMenuItem(page, 'ROI', 'Export ROI Images…');
    await dialogShot(page, 'export-roi-images', page.getByRole('dialog', { name: 'Export ROI Images' }));
    await page.keyboard.press('Escape');
    await page.getByRole('dialog').waitFor({ state: 'hidden' });

    await chooseMenuItem(page, 'File', /^Save Project…/);
    await dialogShot(page, 'save-project', page.getByRole('dialog', { name: 'Save Project' }));
    await page.keyboard.press('Escape');
    await page.getByRole('dialog').waitFor({ state: 'hidden' });

    await chooseMenuItem(page, 'Edit', /^Preferences…/);
    await dialogShot(page, 'preferences', page.getByRole('dialog', { name: 'Preferences' }));
    await page.keyboard.press('Escape');
    await context.close();

    // Access token prompt of a server that requires a token
    const tokenContext = await browser.newContext({ baseURL: `http://127.0.0.1:${TOKEN_PORT}`, viewport: VIEWPORT, locale: 'en-US', colorScheme: 'dark' });
    const tokenPage = await tokenContext.newPage();
    await tokenPage.goto('/');
    await dialogShot(tokenPage, 'token-prompt', tokenPage.getByRole('dialog', { name: 'Access token' }));
    await tokenContext.close();
  } finally {
    await browser.close();
    await stopServer(server);
    await stopServer(tokenServer);
  }
}

await main();
