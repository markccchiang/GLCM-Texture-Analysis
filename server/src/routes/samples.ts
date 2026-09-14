import { createReadStream, type ReadStream } from 'node:fs';
import fs from 'node:fs/promises';
import path from 'node:path';
import type { FastifyPluginAsyncTypebox } from '@fastify/type-provider-typebox';
import { ErrorResponse, SAMPLE_PATH_PATTERN, SampleInfo, SamplesResponse } from '@glcm/api';
import { Type } from 'typebox';
import { ApiError } from '../errors.js';

export interface SampleRoutesOptions {
  samplesDir: string | null;
}

const CONTENT_TYPES: Record<string, string> = {
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.bmp': 'image/bmp',
  '.tif': 'image/tiff',
  '.tiff': 'image/tiff',
};

export const DEFAULT_SAMPLE = 'textures/camera.png';

const samplePathRegex = new RegExp(SAMPLE_PATH_PATTERN);

/** Image files below the samples directory, sorted by path; symbolic links and hidden files are skipped */
export async function listSamples(samplesDir: string): Promise<SampleInfo[]> {
  let entries;
  try {
    entries = await fs.readdir(samplesDir, { recursive: true, withFileTypes: true });
  } catch (error) {
    if ((error as { code?: string }).code === 'ENOENT') {
      return [];
    }
    throw error;
  }

  const samples: SampleInfo[] = [];
  for (const entry of entries) {
    if (!entry.isFile() || !(path.extname(entry.name).toLowerCase() in CONTENT_TYPES)) {
      continue;
    }
    const absolute = path.join(entry.parentPath, entry.name);
    const relative = path.relative(samplesDir, absolute).split(path.sep).join('/');
    if (!samplePathRegex.test(relative)) {
      continue;
    }
    const stat = await fs.stat(absolute);
    samples.push({ path: relative, name: entry.name, group: path.posix.dirname(relative).replace(/^\.$/, ''), sizeBytes: stat.size });
  }
  return samples.sort((a, b) => a.path.localeCompare(b.path));
}

export const sampleRoutes: FastifyPluginAsyncTypebox<SampleRoutesOptions> = async (app, { samplesDir }) => {
  app.get(
    '/samples',
    {
      schema: {
        summary: 'List sample images',
        tags: ['samples'],
        response: { 200: SamplesResponse },
      },
    },
    async () => {
      const samples = samplesDir ? await listSamples(samplesDir) : [];
      return { samples, defaultSample: samples.some((sample) => sample.path === DEFAULT_SAMPLE) ? DEFAULT_SAMPLE : null };
    },
  );

  app.get(
    '/samples/file',
    {
      schema: {
        summary: 'Download a sample image',
        description: 'The web app uploads the downloaded file with POST /images, like a file chosen by the user.',
        tags: ['samples'],
        querystring: Type.Object({ path: Type.String({ maxLength: 1024, description: 'Sample path from GET /samples' }) }),
        response: {
          200: {
            description: 'Image file',
            content: { 'application/octet-stream': { schema: Type.Unsafe<ReadStream>({ type: 'string', format: 'binary' }) } },
          },
          404: ErrorResponse,
        },
      },
    },
    async (request, reply) => {
      const requested = request.query.path;
      // Only paths from the listing are served, so nothing outside the samples directory can be reached
      const sample = samplesDir ? (await listSamples(samplesDir)).find((candidate) => candidate.path === requested) : undefined;
      if (!samplesDir || !sample) {
        throw new ApiError(404, 'NotFound', `Sample ${requested} was not found`);
      }
      reply
        .type(CONTENT_TYPES[path.extname(sample.name).toLowerCase()])
        .header('Content-Length', String(sample.sizeBytes))
        .header('Cache-Control', 'no-cache');
      return reply.send(createReadStream(path.join(samplesDir, ...sample.path.split('/'))));
    },
  );
};
