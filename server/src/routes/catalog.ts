import type { FastifyPluginAsyncTypebox } from '@fastify/type-provider-typebox';
import { CatalogResponse } from '@glcm/api';
import * as native from '@glcm/native';
import type { ServerConfig } from '../config.js';

export interface CatalogRoutesOptions {
  config: ServerConfig;
}

export const catalogRoutes: FastifyPluginAsyncTypebox<CatalogRoutesOptions> = async (app, { config }) => {
  // The catalog is fixed for a given core build
  const catalog = native.catalog();

  app.get(
    '/catalog',
    {
      schema: {
        summary: 'Features, presets and limits',
        description: 'Everything the web app needs to build its feature picker and validate settings before sending them.',
        tags: ['system'],
        response: { 200: CatalogResponse },
      },
    },
    async () => ({
      ...catalog,
      uploads: {
        maxUploadBytes: config.maxUploadBytes,
        maxImagePixels: config.maxImagePixels,
        rawTransferMaxPixels: config.rawTransferMaxPixels,
        displayMaxSize: config.displayMaxSize,
      },
    }),
  );
};
