// Token authentication, CORS and rate limits (doc/ui-design-plan.md, section 8.2).

import { createHash, timingSafeEqual } from 'node:crypto';
import cors from '@fastify/cors';
import rateLimit from '@fastify/rate-limit';
import { API_PREFIX, RAW_HEADERS } from '@glcm/api';
import type { FastifyInstance, FastifyRequest } from 'fastify';
import type { ServerConfig } from './config.js';
import { ApiError } from './errors.js';

const HEALTH_PATH = `${API_PREFIX}/health`;

function digest(text: string): Buffer {
  return createHash('sha256').update(text).digest();
}

/** The token of an "Authorization: Bearer <token>" header, or null */
export function bearerToken(header: string | undefined): string | null {
  const match = /^Bearer[ \t]+(\S+)[ \t]*$/i.exec(header ?? '');
  return match ? match[1] : null;
}

function isApiRequest(request: FastifyRequest): boolean {
  const url = request.url.split('?')[0];
  return url === API_PREFIX || url.startsWith(`${API_PREFIX}/`);
}

/**
 * Requires the bearer token on every API request except GET /health and CORS preflights. Digests of equal length are
 * compared in constant time; failures get a 401 without details. The web app itself stays public, so it can ask for the
 * token.
 */
export function registerAuthentication(app: FastifyInstance, token: string): void {
  const expected = digest(token);
  app.addHook('onRequest', async (request, reply) => {
    if (!isApiRequest(request) || request.method === 'OPTIONS' || request.url.split('?')[0] === HEALTH_PATH) {
      return;
    }
    const presented = bearerToken(request.headers.authorization);
    if (presented === null || !timingSafeEqual(digest(presented), expected)) {
      reply.header('WWW-Authenticate', 'Bearer');
      throw new ApiError(401, 'Unauthorized', 'Authentication required');
    }
  });
}

export async function registerCors(app: FastifyInstance, origins: string[]): Promise<void> {
  await app.register(cors, {
    origin: origins,
    methods: ['GET', 'HEAD', 'POST', 'DELETE'],
    allowedHeaders: ['Authorization', 'Content-Type', 'Accept', 'If-None-Match'],
    exposedHeaders: ['Content-Disposition', 'ETag', ...Object.values(RAW_HEADERS)],
    maxAge: 600,
  });
}

export async function registerRateLimit(app: FastifyInstance, config: Pick<ServerConfig, 'rateLimitPerMinute'>): Promise<void> {
  await app.register(rateLimit, {
    global: true,
    max: config.rateLimitPerMinute,
    timeWindow: 60_000,
    // Per token when one is sent (hashed, so tokens never become cache keys), otherwise per client address
    keyGenerator: (request) => {
      const token = bearerToken(request.headers.authorization);
      return token ? `token:${digest(token).toString('hex')}` : `ip:${request.ip}`;
    },
    // The web app's static files are not limited
    allowList: (request) => !isApiRequest(request),
    errorResponseBuilder: (_request, context) =>
      new ApiError(429, 'TooManyRequests', `More than ${context.max} requests per minute; retry in ${context.after}`),
  });
}
