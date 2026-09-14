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
import type { ImageStore } from '../storage/ImageStore.js';

export interface AnalysisRoutesOptions {
  store: ImageStore;
  jobs: JobManager;
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

export const analysisRoutes: FastifyPluginAsyncTypebox<AnalysisRoutesOptions> = async (app, { store, jobs, heartbeatMs = 15000 }) => {
  async function requireImage(imageId: string): Promise<ImageInfo> {
    const info = await store.info(imageId);
    if (!info) {
      throw new ApiError(404, 'NotFound', `Image ${imageId} was not found`);
    }
    return info;
  }

  function requireAnalysis(analysisId: string): AnalysisState {
    const state = jobs.get(analysisId);
    if (!state) {
      throw new ApiError(404, 'NotFound', `Analysis ${analysisId} was not found`);
    }
    return state;
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
    async (request) => requireAnalysis(request.params.id).info,
  );

  app.delete(
    '/analyses/:id',
    {
      schema: {
        summary: 'Cancel an analysis',
        description: 'Queued jobs are dropped; running jobs finish and their results are kept.',
        tags: ['analyses'],
        params: AnalysisIdParams,
        response: { 204: NoBody('Cancelled (no body)'), 400: ErrorResponse, 404: ErrorResponse },
      },
    },
    async (request, reply) => {
      if (!jobs.cancel(request.params.id)) {
        throw new ApiError(404, 'NotFound', `Analysis ${request.params.id} was not found`);
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
    async (request) => jobs.results(requireAnalysis(request.params.id)),
  );

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
      const state = requireAnalysis(request.params.id);
      const response = reply.raw;
      reply.hijack();
      response.writeHead(200, {
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
