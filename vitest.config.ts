import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    include: ['bindings/node/test/**/*.test.ts', 'server/test/**/*.test.ts'],
    // Separate processes: the native addon is loaded once per test file
    pool: 'forks',
    testTimeout: 30_000,
  },
});
