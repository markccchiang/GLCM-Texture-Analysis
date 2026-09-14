// End-to-end tests (doc/ui-design-plan.md, section 8.3): npm run test:e2e builds the web app and starts two servers,
// one without authentication (local mode) and one that requires an access token.

import os from 'node:os';
import path from 'node:path';
import { defineConfig, devices } from '@playwright/test';
import { E2E_API_TOKEN } from './e2e/token.js';

const PORT = 8181;
const TOKEN_PORT = 8182;
const VIEWPORT = { width: 1440, height: 900 };
const TOKEN_TESTS = /auth\.spec\.ts/;

export default defineConfig({
  testDir: 'e2e',
  timeout: 60_000,
  expect: { timeout: 15_000 },
  // One server and one analysis queue per mode: run the tests one after another
  workers: 1,
  reporter: [['list']],
  use: {
    baseURL: `http://127.0.0.1:${PORT}`,
    locale: 'en-US',
    trace: 'retain-on-failure',
  },
  projects: [
    { name: 'chromium', testIgnore: TOKEN_TESTS, use: { ...devices['Desktop Chrome'], viewport: VIEWPORT } },
    { name: 'webkit', testIgnore: TOKEN_TESTS, use: { ...devices['Desktop Safari'], viewport: VIEWPORT } },
    {
      name: 'chromium-token',
      testMatch: TOKEN_TESTS,
      use: { ...devices['Desktop Chrome'], viewport: VIEWPORT, baseURL: `http://127.0.0.1:${TOKEN_PORT}` },
    },
    {
      name: 'webkit-token',
      testMatch: TOKEN_TESTS,
      use: { ...devices['Desktop Safari'], viewport: VIEWPORT, baseURL: `http://127.0.0.1:${TOKEN_PORT}` },
    },
  ],
  webServer: [
    {
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
    {
      command: 'npm start',
      url: `http://127.0.0.1:${TOKEN_PORT}/api/v1/health`,
      reuseExistingServer: false,
      timeout: 120_000,
      env: {
        GLCM_PORT: String(TOKEN_PORT),
        GLCM_DATA_DIR: path.join(os.tmpdir(), 'glcm-e2e-token-data'),
        GLCM_API_TOKEN: E2E_API_TOKEN,
        GLCM_LOG_LEVEL: 'warn',
      },
    },
  ],
});
