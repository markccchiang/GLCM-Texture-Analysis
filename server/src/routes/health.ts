import type { FastifyPluginAsyncTypebox } from '@fastify/type-provider-typebox';
import { HealthResponse } from '@glcm/api';
import * as native from '@glcm/native';

export const healthRoutes: FastifyPluginAsyncTypebox = async (app) => {
  app.get(
    '/health',
    { schema: { summary: 'Liveness check', tags: ['system'], response: { 200: HealthResponse } } },
    async () => ({ status: 'ok' as const, coreVersion: native.coreVersion() }),
  );
};
