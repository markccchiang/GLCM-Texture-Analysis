import fs from 'node:fs';
import path from 'node:path';
import fastifyStatic from '@fastify/static';
import { API_PREFIX } from '@glcm/api';
import type { FastifyInstance, FastifyReply, FastifyRequest } from 'fastify';

/** URL prefix of the built documentation, e.g. /docs/equations.html#haralick-features */
export const DOCS_PREFIX = '/docs/';

/** Whether a directory holds built Sphinx documentation */
export function hasDocs(docsDir: string | null): docsDir is string {
  return docsDir !== null && fs.existsSync(path.join(docsDir, 'index.html'));
}

/** Serves the built documentation, so the web app can link to the feature equations (Help ▸ Feature Equations) */
export async function registerDocs(app: FastifyInstance, docsDir: string): Promise<void> {
  await app.register(fastifyStatic, {
    root: docsDir,
    prefix: DOCS_PREFIX,
    // The web app's registration already added reply.sendFile
    decorateReply: false,
    wildcard: true,
    index: 'index.html',
    setHeaders(reply) {
      reply.header('Cache-Control', 'no-cache');
    },
  });
}

/** Whether a directory holds a built web app */
export function hasWebApp(webDir: string | null): webDir is string {
  return webDir !== null && fs.existsSync(path.join(webDir, 'index.html'));
}

/**
 * Serves the built web app. Unknown GET requests outside the API that accept HTML get index.html, so client-side
 * routes survive a reload; everything else keeps the JSON 404.
 */
export async function registerWebApp(app: FastifyInstance, webDir: string): Promise<void> {
  await app.register(fastifyStatic, {
    root: webDir,
    // Look files up per request, so a rebuilt app (new hashed asset names) is served without restarting. Missing
    // files fall through to the not-found handler.
    wildcard: true,
    index: 'index.html',
    setHeaders(reply, filePath) {
      // Vite puts content hashes in asset names; index.html must be revalidated to pick up new builds
      const immutable = filePath.startsWith(path.join(webDir, 'assets') + path.sep);
      reply.header('Cache-Control', immutable ? 'public, max-age=31536000, immutable' : 'no-cache');
    },
  });
}

export function wantsWebApp(request: FastifyRequest): boolean {
  const url = request.url.split('?')[0];
  const isApi = url === API_PREFIX || url.startsWith(`${API_PREFIX}/`) || url === '/api' || url.startsWith('/api/');
  return (request.method === 'GET' || request.method === 'HEAD') && !isApi && (request.headers.accept ?? '').includes('text/html');
}

export function sendWebApp(reply: FastifyReply) {
  return reply.header('Cache-Control', 'no-cache').sendFile('index.html');
}
