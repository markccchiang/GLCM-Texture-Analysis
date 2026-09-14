import { EventEmitter } from 'node:events';
import type { ServerResponse } from 'node:http';
import type { FastifyPluginAsyncTypebox } from '@fastify/type-provider-typebox';
import {
  AnalysisIdParams,
  AnalysisInfo,
  AnalysisRequest,
  AnalysisResults,
  ErrorResponse,
  ImageIdParams,
  RoiStatsRequest,
  RoiStatsResponse,
  type AnalysisEvent,
  type ImageInfo,
} from '@glcm/api';
import * as native from '@glcm/native';
import { Type } from 'typebox';
import { isFinished, type AnalysisState, type JobManager } from '../analysis/JobManager.js';
import { ApiError } from '../errors.js';
import { attachment, fileStem } from '../files.js';
import type { ImageStore } from '../storage/ImageStore.js';
import type { ResultStore } from '../storage/ResultStore.js';
import { formatResultsDocument } from './exports.js';

export interface AnalysisRoutesOptions {
  store: ImageStore;
  jobs: JobManager;
  results: ResultStore;
  /** Interval of SSE keep-alive comments */
  heartbeatMs?: number;
}

const NoBody = (description: string) => Type.Unsafe<undefined>({ type: 'null', description });

function nativeError(error: unknown): never {
  if ((error as { code?: string }).code === 'INVALID_ARGUMENT') {
    throw new ApiError(400, 'BadRequest', (error as Error).message);
  }
  throw error;
}

function writeEvent(response: ServerResponse, { event, data }: AnalysisEvent): void {
  response.write(`event: ${event}\ndata: ${JSON.stringify(data)}\n\n`);
}

export const analysisRoutes: FastifyPluginAsyncTypebox<AnalysisRoutesOptions> = async (app, { store, jobs, results, heartbeatMs = 15000 }) => {
  async function requireImage(imageId: string): Promise<ImageInfo> {
    const info = await store.info(imageId);
    if (!info) {
      throw new ApiError(404, 'NotFound', `Image ${imageId} was not found`);
    }
    return info;
  }

  /** A running or recently finished analysis from memory, or a finished one stored on disk */
  async function requireAnalysis(analysisId: string): Promise<AnalysisState> {
    const state = jobs.get(analysisId);
    if (state) {
      return state;
    }
    const stored = await results.load(analysisId);
    if (!stored) {
      throw new ApiError(404, 'NotFound', `Analysis ${analysisId} was not found`);
    }
    return { info: stored.info, results: stored.results.results, events: new EventEmitter() };
  }

  app.post(
    '/images/:id/roi-stats',
    {
      schema: {
        summary: 'Pixel count and intensity statistics of ROIs',
        description: 'Masks follow the pixel-centre rule of glcm::RasterizeMask, so the counts equal those used by analyses.',
        tags: ['rois'],
        params: ImageIdParams,
        body: RoiStatsRequest,
        response: { 200: RoiStatsResponse, 400: ErrorResponse, 404: ErrorResponse },
      },
    },
    async (request) => {
      const info = await requireImage(request.params.id);
      const { rois } = request.body;
      if (rois.length === 0) {
        return { stats: [] };
      }
      const pixels = await store.pixels(info.imageId);
      const stats = await native
        .roiStats(pixels, info.width, info.height, info.bitDepth, JSON.stringify(rois.map(({ id, shape }) => ({ id, shape }))))
        .catch(nativeError);
      return { stats: stats.map((statistics, i) => ({ roiId: rois[i].id, ...statistics })) };
    },
  );

  app.post(
    '/analyses',
    {
      schema: {
        summary: 'Start an analysis',
        description: 'Measures every ROI at every distance. Progress and results are streamed by GET /analyses/{id}/events.',
        tags: ['analyses'],
        body: AnalysisRequest,
        response: { 202: AnalysisInfo, 400: ErrorResponse, 404: ErrorResponse },
      },
    },
    async (request, reply) => {
      const { imageId, rois, settings } = request.body;
      const image = await requireImage(imageId);
      try {
        native.validateAnalysis(JSON.stringify(rois), JSON.stringify(settings));
      } catch (error) {
        nativeError(error);
      }
      const state = jobs.start(request.body, image, await store.pixels(imageId));
      return reply.code(202).send(state.info);
    },
  );

  app.get(
    '/analyses/:id',
    {
      schema: {
        summary: 'Analysis status',
        tags: ['analyses'],
        params: AnalysisIdParams,
        response: { 200: AnalysisInfo, 400: ErrorResponse, 404: ErrorResponse },
      },
    },
    async (request) => (await requireAnalysis(request.params.id)).info,
  );

  app.delete(
    '/analyses/:id',
    {
      schema: {
        summary: 'Cancel an analysis',
        description: 'Queued jobs are dropped; running jobs finish and their results are kept. Finished analyses are not changed.',
        tags: ['analyses'],
        params: AnalysisIdParams,
        response: { 204: NoBody('Cancelled (no body)'), 400: ErrorResponse, 404: ErrorResponse },
      },
    },
    async (request, reply) => {
      if (!jobs.cancel(request.params.id)) {
        // A finished analysis stored on disk cannot be cancelled any more
        await requireAnalysis(request.params.id);
      }
      return reply.code(204).send(undefined);
    },
  );

  app.get(
    '/analyses/:id/results',
    {
      schema: {
        summary: 'Results of finished jobs',
        tags: ['analyses'],
        params: AnalysisIdParams,
        response: { 200: AnalysisResults, 400: ErrorResponse, 404: ErrorResponse },
      },
    },
    async (request) => jobs.results(await requireAnalysis(request.params.id)),
  );

  for (const format of ['csv', 'json'] as const) {
    app.get(
      `/analyses/:id/results.${format}`,
      {
        schema: {
          summary: `Results of finished jobs as a ${format.toUpperCase()} file`,
          description: 'Written by glcm_core; the same content as POST /exports/results for this analysis.',
          tags: ['analyses'],
          params: AnalysisIdParams,
          response: {
            200: {
              description: `${format.toUpperCase()} file`,
              content: { [format === 'csv' ? 'text/csv' : 'application/json']: { schema: Type.Unsafe<Buffer>({ type: 'string', format: 'binary' }) } },
            },
            400: ErrorResponse,
            404: ErrorResponse,
          },
        },
      },
      async (request, reply) => {
        const { timestamp, image, settings, results: measurements } = jobs.results(await requireAnalysis(request.params.id));
        const text = formatResultsDocument({ timestamp, image: { name: image.name, sha256: image.sha256 }, settings, results: measurements }, format);
        return reply
          .header('Content-Disposition', attachment(`${fileStem(image.name)}-results.${format}`))
          .type(format === 'csv' ? 'text/csv; charset=utf-8' : 'application/json; charset=utf-8')
          .send(Buffer.from(text));
      },
    );
  }

  app.get(
    '/analyses/:id/events',
    {
      schema: {
        summary: 'Progress as Server-Sent Events',
        description:
          'Events: "result" {index, result} for every finished job (earlier ones are replayed on connect), "progress" {completed, total}, and a final "finished" {status, completed, total, error}, after which the stream ends.',
        tags: ['analyses'],
        params: AnalysisIdParams,
        response: {
          200: { description: 'Event stream', content: { 'text/event-stream': { schema: Type.Unsafe<string>({ type: 'string' }) } } },
          400: ErrorResponse,
          404: ErrorResponse,
        },
      },
    },
    async (request, reply) => {
      const state = await requireAnalysis(request.params.id);
      const response = reply.raw;
      reply.hijack();
      response.writeHead(200, {
        // Hijacked responses skip Fastify's hooks, so headers set by plugins (e.g. CORS) are copied here
        ...(reply.getHeaders() as Record<string, string>),
        'Content-Type': 'text/event-stream; charset=utf-8',
        'Cache-Control': 'no-store',
        Connection: 'keep-alive',
        'X-Accel-Buffering': 'no',
      });

      state.results.forEach((result, index) => {
        if (result) {
          writeEvent(response, { event: 'result', data: { index, result } });
        }
      });
      const { info } = state;
      writeEvent(response, { event: 'progress', data: { completed: info.completed, total: info.total } });
      if (isFinished(info.status)) {
        writeEvent(response, { event: 'finished', data: { status: info.status, completed: info.completed, total: info.total, error: info.error } });
        response.end();
        return;
      }

      const onEvent = (event: AnalysisEvent) => {
        writeEvent(response, event);
        if (event.event === 'finished') {
          cleanup();
          response.end();
        }
      };
      const heartbeat = setInterval(() => response.write(': keep-alive\n\n'), heartbeatMs);
      const cleanup = () => {
        clearInterval(heartbeat);
        state.events.off('event', onEvent);
      };
      state.events.on('event', onEvent);
      request.raw.on('close', cleanup);
    },
  );
};
