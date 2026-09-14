// Analysis jobs (doc/ui-design-plan.md, section 6.3.3). Each ROI × distance pair is one job, run with the addon's
// AsyncWorker. Jobs of all analyses share one queue, so at most `concurrency` of them run at a time.

import { randomUUID } from 'node:crypto';
import { EventEmitter } from 'node:events';
import type {
  AnalysisEvent,
  AnalysisInfo,
  AnalysisRequest,
  AnalysisResults,
  AnalysisStatus,
  ImageInfo,
  MeasurementResult,
  Roi,
} from '@glcm/api';
import * as native from '@glcm/native';

export function newAnalysisId(): string {
  return `ana_${randomUUID().replaceAll('-', '')}`;
}

interface Job {
  /** Position in the result order: ROI index × distance count + distance index */
  index: number;
  roi: Roi;
  distance: number;
}

export interface AnalysisState {
  info: AnalysisInfo;
  /** Finished jobs by index */
  results: Array<MeasurementResult | undefined>;
  /** Emits 'event' with AnalysisEvent values */
  events: EventEmitter;
}

interface InternalState extends AnalysisState {
  image: ImageInfo;
  request: AnalysisRequest;
  /** Released when the analysis finishes */
  pixels: Buffer | null;
  running: number;
  cancelRequested: boolean;
}

export interface JobManagerOptions {
  /** Jobs running at the same time */
  concurrency: number;
  /** Finished analyses kept in memory; the oldest are forgotten first */
  retainFinished: number;
}

const FINISHED: ReadonlySet<AnalysisStatus> = new Set(['completed', 'cancelled', 'failed']);

export function isFinished(status: AnalysisStatus): boolean {
  return FINISHED.has(status);
}

function failedResult(job: Job, message: string): MeasurementResult {
  return {
    roiId: job.roi.id,
    roiName: job.roi.name,
    distance: job.distance,
    status: 'failed',
    error: message,
    pixelCount: 0,
    pairCounts: { '0': 0, '45': 0, '90': 0, '135': 0 },
    quantization: { lower: 0, upper: 0 },
    values: {},
    score: null,
    warnings: [],
  };
}

export class JobManager {
  private readonly analyses = new Map<string, InternalState>();
  private readonly queue: Array<{ state: InternalState; job: Job }> = [];
  private running = 0;

  constructor(private readonly options: JobManagerOptions) {}

  /** Queues the jobs of an analysis; the request must already be validated */
  start(request: AnalysisRequest, image: ImageInfo, pixels: Buffer): AnalysisState {
    const { distances } = request.settings;
    const jobs: Job[] = [];
    request.rois.forEach((roi, roiIndex) => {
      distances.forEach((distance, distanceIndex) => jobs.push({ index: roiIndex * distances.length + distanceIndex, roi, distance }));
    });

    const events = new EventEmitter();
    events.setMaxListeners(0);
    const state: InternalState = {
      info: {
        analysisId: newAnalysisId(),
        imageId: image.imageId,
        imageName: image.name,
        imageSha256: image.sha256,
        status: 'queued',
        total: jobs.length,
        completed: 0,
        error: null,
        createdAt: new Date().toISOString(),
        finishedAt: null,
        coreVersion: native.coreVersion(),
        settings: request.settings,
      },
      results: new Array<MeasurementResult | undefined>(jobs.length),
      events,
      image,
      request,
      pixels,
      running: 0,
      cancelRequested: false,
    };
    this.analyses.set(state.info.analysisId, state);
    this.forgetOldAnalyses();

    for (const job of jobs) {
      this.queue.push({ state, job });
    }
    this.pump();
    return state;
  }

  get(analysisId: string): AnalysisState | undefined {
    return this.analyses.get(analysisId);
  }

  /** Drops the queued jobs of an analysis; running jobs finish, then it is marked cancelled. False if unknown. */
  cancel(analysisId: string): boolean {
    const state = this.analyses.get(analysisId);
    if (!state) {
      return false;
    }
    if (isFinished(state.info.status)) {
      return true;
    }
    state.cancelRequested = true;
    for (let i = this.queue.length - 1; i >= 0; i -= 1) {
      if (this.queue[i].state === state) {
        this.queue.splice(i, 1);
      }
    }
    if (state.running === 0) {
      this.finish(state, 'cancelled');
    }
    return true;
  }

  results(state: AnalysisState): AnalysisResults {
    return {
      format: 'glcm-results',
      version: 1,
      analysisId: state.info.analysisId,
      status: state.info.status,
      coreVersion: state.info.coreVersion,
      timestamp: state.info.finishedAt ?? new Date().toISOString(),
      image: { id: state.info.imageId, name: state.info.imageName, sha256: state.info.imageSha256 },
      settings: state.info.settings,
      results: state.results.filter((result): result is MeasurementResult => result !== undefined),
    };
  }

  /** Number of queued or running jobs */
  get pendingJobs(): number {
    return this.queue.length + this.running;
  }

  private emit(state: AnalysisState, event: AnalysisEvent): void {
    state.events.emit('event', event);
  }

  private pump(): void {
    while (this.running < this.options.concurrency && this.queue.length > 0) {
      const { state, job } = this.queue.shift()!;
      void this.run(state, job);
    }
  }

  private async run(state: InternalState, job: Job): Promise<void> {
    this.running += 1;
    state.running += 1;
    if (state.info.status === 'queued') {
      state.info.status = 'running';
    }

    let result: MeasurementResult;
    try {
      const { image, request } = state;
      const json = await native.runAnalysis(
        state.pixels!,
        image.width,
        image.height,
        image.bitDepth,
        JSON.stringify([job.roi]),
        JSON.stringify({ ...request.settings, distances: [job.distance] }),
      );
      result = (JSON.parse(json) as { results: MeasurementResult[] }).results[0];
    } catch (error) {
      result = failedResult(job, error instanceof Error ? error.message : String(error));
    } finally {
      this.running -= 1;
      state.running -= 1;
    }

    state.results[job.index] = result;
    state.info.completed += 1;
    this.emit(state, { event: 'result', data: { index: job.index, result } });
    this.emit(state, { event: 'progress', data: { completed: state.info.completed, total: state.info.total } });

    if (state.info.completed === state.info.total) {
      this.finish(state, 'completed');
    } else if (state.cancelRequested && state.running === 0) {
      this.finish(state, 'cancelled');
    }
    this.pump();
  }

  private finish(state: InternalState, status: AnalysisStatus, error: string | null = null): void {
    state.info.status = status;
    state.info.error = error;
    state.info.finishedAt = new Date().toISOString();
    state.pixels = null;
    this.emit(state, {
      event: 'finished',
      data: { status, completed: state.info.completed, total: state.info.total, error },
    });
  }

  private forgetOldAnalyses(): void {
    const finished = [...this.analyses.values()].filter((state) => isFinished(state.info.status));
    for (const state of finished.slice(0, Math.max(0, finished.length - this.options.retainFinished))) {
      this.analyses.delete(state.info.analysisId);
    }
  }
}
