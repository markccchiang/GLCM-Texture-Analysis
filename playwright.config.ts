// End-to-end tests (doc/ui-design-plan.md, section 8.3): npm run test:e2e builds the web app and starts the server.

import os from 'node:os';
import path from 'node:path';
import { defineConfig, devices } from '@playwright/test';

const PORT = 8181;
const VIEWPORT = { width: 1440, height: 900 };

export default defineConfig({
  testDir: 'e2e',
  timeout: 60_000,
  expect: { timeout: 15_000 },
  // One server and one analysis queue: run the tests one after another
  workers: 1,
  reporter: [['list']],
  use: {
    baseURL: `http://127.0.0.1:${PORT}`,
    locale: 'en-US',
    trace: 'retain-on-failure',
  },
  projects: [
    { name: 'chromium', use: { ...devices['Desktop Chrome'], viewport: VIEWPORT } },
    { name: 'webkit', use: { ...devices['Desktop Safari'], viewport: VIEWPORT } },
  ],
  webServer: {
    command: 'npm start',
    url: `http://127.0.0.1:${PORT}/api/v1/health`,
    reuseExistingServer: false,
    timeout: 120_000,
    env: {
      GLCM_PORT: String(PORT),
      GLCM_DATA_DIR: path.join(os.tmpdir(), 'glcm-e2e-data'),
      GLCM_LOG_LEVEL: 'warn',
    },
  },
});
