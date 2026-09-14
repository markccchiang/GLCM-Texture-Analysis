import { buildApp } from './app.js';
import { isLoopbackHost, loadConfig } from './config.js';

const config = loadConfig();

// Server mode (any other address) needs token authentication, which arrives in phase 5 of doc/ui-design-plan.md
if (!isLoopbackHost(config.host)) {
  console.error(
    `Refusing to listen on ${config.host}: only local mode (127.0.0.1, ::1 or localhost) is available until token authentication is implemented.`,
  );
  process.exit(1);
}

const app = await buildApp(config);
try {
  await app.listen({ host: config.host, port: config.port });
  app.log.info(`Data directory: ${config.dataDir}`);
} catch (error) {
  app.log.error(error);
  process.exit(1);
}
