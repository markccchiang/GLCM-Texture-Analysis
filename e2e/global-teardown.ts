// Removes the data directories of this test run, created in playwright.config.ts
import fs from 'node:fs/promises';

export default async function globalTeardown(): Promise<void> {
  const directory = process.env.GLCM_E2E_DATA_DIR;
  if (directory) {
    await fs.rm(directory, { recursive: true, force: true, maxRetries: 3 });
  }
}
