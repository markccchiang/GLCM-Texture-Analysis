import { buildApp } from './app.js';
import { loadConfig, serverMode, validateConfig, type ServerConfig } from './config.js';

let config: ServerConfig;
try {
  config = loadConfig();
  // Server mode (any address other than loopback) requires a strong GLCM_API_TOKEN (doc/ui-design-plan.md, section 8.2)
  validateConfig(config);
} catch (error) {
  console.error(`Invalid configuration: ${(error as Error).message}`);
  process.exit(1);
}

const app = await buildApp(config, { retention: true });

// Containers stop with SIGTERM: finish open requests and close the server
for (const signal of ['SIGTERM', 'SIGINT'] as const) {
  process.once(signal, async () => {
    app.log.info(`${signal} received; shutting down`);
    await app.close();
    process.exit(0);
  });
}

try {
  await app.listen({ host: config.host, port: config.port });
  app.log.info(
    `Mode: ${serverMode(config)}; authentication: ${config.apiToken ? 'bearer token' : 'none'}; retention: ${
      config.retentionHours > 0 ? `${config.retentionHours} h` : 'off'
    }; data directory: ${config.dataDir}`,
  );
} catch (error) {
  app.log.error(error);
  process.exit(1);
}
