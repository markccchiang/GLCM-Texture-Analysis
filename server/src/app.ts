import path from 'node:path';
import multipart from '@fastify/multipart';
import swagger from '@fastify/swagger';
import type { TypeBoxTypeProvider } from '@fastify/type-provider-typebox';
import { API_PREFIX } from '@glcm/api';
import * as native from '@glcm/native';
import Fastify from 'fastify';
import type { ServerConfig } from './config.js';
import { ApiError } from './errors.js';
import { catalogRoutes } from './routes/catalog.js';
import { healthRoutes } from './routes/health.js';
import { imageRoutes } from './routes/images.js';
import { DisplayCache } from './storage/DisplayCache.js';
import { ImageStore } from './storage/ImageStore.js';

export interface BuildAppOptions {
  /** false disables request logging (tests, OpenAPI generation) */
  logger?: boolean;
}

export async function buildApp(config: ServerConfig, options: BuildAppOptions = {}) {
  const app = Fastify({
    logger: options.logger === false ? false : { level: config.logLevel },
    // JSON bodies are small; images arrive as multipart uploads with their own limit
    bodyLimit: 1024 * 1024,
  }).withTypeProvider<TypeBoxTypeProvider>();

  const store = new ImageStore(config.dataDir);
  await store.init();
  const displayCache = new DisplayCache(path.join(config.dataDir, 'cache', 'display'), config.displayCacheBytes);
  await displayCache.init();

  await app.register(swagger, {
    openapi: {
      openapi: '3.1.0',
      info: {
        title: 'GLCM Texture Analysis API',
        description: 'HTTP API of the GLCM texture analysis server (doc/ui-design-plan.md, section 8.5).',
        version: native.coreVersion(),
      },
      tags: [
        { name: 'system', description: 'Health and feature catalog' },
        { name: 'images', description: 'Upload, display and pixel data' },
      ],
    },
  });

  await app.register(multipart, {
    limits: { fileSize: config.maxUploadBytes, files: 1, fields: 10, parts: 11 },
  });

  app.setErrorHandler((error, request, reply) => {
    if (error instanceof ApiError) {
      return reply.code(error.statusCode).send({ error: error.code, message: error.message });
    }
    const fastifyError = error as { validation?: unknown; code?: string; statusCode?: number; message: string };
    if (fastifyError.validation) {
      return reply.code(400).send({ error: 'BadRequest', message: fastifyError.message });
    }
    if (fastifyError.code === 'FST_REQ_FILE_TOO_LARGE') {
      return reply.code(413).send({ error: 'PayloadTooLarge', message: `The file is larger than the limit of ${config.maxUploadBytes} bytes` });
    }
    if (fastifyError.statusCode && fastifyError.statusCode >= 400 && fastifyError.statusCode < 500) {
      return reply.code(fastifyError.statusCode).send({ error: fastifyError.code ?? 'BadRequest', message: fastifyError.message });
    }
    request.log.error(error);
    return reply.code(500).send({ error: 'InternalError', message: 'Internal server error' });
  });

  app.setNotFoundHandler((request, reply) =>
    reply.code(404).send({ error: 'NotFound', message: `Route ${request.method} ${request.url} was not found` }),
  );

  await app.register(
    async (api) => {
      await api.register(healthRoutes);
      await api.register(catalogRoutes, { config });
      await api.register(imageRoutes, { config, store, displayCache });
    },
    { prefix: API_PREFIX },
  );

  return app;
}

export type App = Awaited<ReturnType<typeof buildApp>>;
