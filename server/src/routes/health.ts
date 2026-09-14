import type { FastifyPluginAsyncTypebox } from '@fastify/type-provider-typebox';
import { HealthResponse } from '@glcm/api';
import * as native from '@glcm/native';
import { serverMode, type ServerConfig } from '../config.js';

export interface HealthRoutesOptions {
  config: ServerConfig;
}

export const healthRoutes: FastifyPluginAsyncTypebox<HealthRoutesOptions> = async (app, { config }) => {
  app.get(
    '/health',
    {
      schema: {
        summary: 'Liveness check',
        description: 'The only API route that never needs the bearer token; tells the web app whether to ask for one.',
        tags: ['system'],
        security: [],
        response: { 200: HealthResponse },
      },
    },
    async () => ({
      status: 'ok' as const,
      coreVersion: native.coreVersion(),
      mode: serverMode(config),
      authentication: config.apiToken ? ('bearer' as const) : ('none' as const),
    }),
  );
};
