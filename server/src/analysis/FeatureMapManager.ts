// Feature maps: the rows of a map are split into bands, each computed by the addon's computeFeatureMap on the shared
// Scheduler, so maps and analyses share the worker limit. Maps are kept in memory only.

import { randomUUID } from 'node:crypto';
import type { FeatureMapInfo, FeatureMapSettings, FeatureMapStatus, ImageInfo } from '@glcm/api';
import * as native from '@glcm/native';
import { JobLimitError } from './JobManager.js';
import type { Scheduler } from './Scheduler.js';

/** A map is split into at most this many bands of rows, which sets how often its progress advances */
export const MAX_FEATURE_MAP_BANDS = 64;

export function newFeatureMapId(): string {
  return `fmap_${randomUUID().replaceAll('-', '')}`;
}

export interface FeatureMapState {
  info: FeatureMapInfo;
  /** rows × columns values, row-major; NaN until computed and where a window has no pixel pairs */
  values: Float32Array;
}

interface InternalState extends FeatureMapState {
  image: ImageInfo;
  /** Released when the map finishes */
  pixels: Buffer | null;
  running: number;
  queued: number;
  cancelRequested: boolean;
}

export interface FeatureMapManagerOptions {
  scheduler: Scheduler;
  /** Tasks queued or running over all analyses and maps; start() refuses maps that do not fit */
  maxPendingJobs: number;
  /** Finished maps kept in memory; the oldest are forgotten first */
  retainFinished: number;
}

const FINISHED: ReadonlySet<FeatureMapStatus> = new Set(['completed', 'cancelled', 'failed']);

export class FeatureMapManager {
  private readonly maps = new Map<string, InternalState>();

  constructor(private readonly options: FeatureMapManagerOptions) {}

  /**
   * Queues the bands of a map; throws an Error with code INVALID_ARGUMENT for invalid settings and JobLimitError when the
   * bands do not fit into maxPendingJobs
   */
  start(settings: FeatureMapSettings, image: ImageInfo, pixels: Buffer): FeatureMapState {
    const settingsJson = JSON.stringify(settings);
    const grid = native.featureMapGrid(settingsJson, image.width, image.height);
    const bandRows = Math.ceil(grid.rows / MAX_FEATURE_MAP_BANDS);
    const bands = Math.ceil(grid.rows / bandRows);
    const { scheduler, maxPendingJobs } = this.options;
    if (bands > maxPendingJobs) {
      throw new JobLimitError('tooLarge', bands, scheduler.pending, maxPendingJobs);
    }
    if (scheduler.pending + bands > maxPendingJobs) {
      throw new JobLimitError('busy', bands, scheduler.pending, maxPendingJobs);
    }

    const state: InternalState = {
      info: {
        featureMapId: newFeatureMapId(),
        imageId: image.imageId,
        imageName: image.name,
        status: 'queued',
        settings,
        step: grid.step,
        columns: grid.columns,
        rows: grid.rows,
        completedRows: 0,
        error: null,
        createdAt: new Date().toISOString(),
        finishedAt: null,
        coreVersion: native.coreVersion(),
      },
      values: new Float32Array(grid.rows * grid.columns).fill(Number.NaN),
      image,
      pixels,
      running: 0,
      queued: bands,
      cancelRequested: false,
    };
    this.maps.set(state.info.featureMapId, state);
    this.forgetOldMaps();

    const tasks = Array.from({ length: bands }, (_, band) => {
      const firstRow = band * bandRows;
      return (release: () => void) => this.runBand(state, settingsJson, firstRow, Math.min(bandRows, grid.rows - firstRow), release);
    });
    scheduler.enqueue(state, tasks);
    return state;
  }

  get(featureMapId: string): FeatureMapState | undefined {
    return this.maps.get(featureMapId);
  }

  /** Cancels a queued or running map, or forgets a finished one. False if unknown. */
  cancel(featureMapId: string): boolean {
    const state = this.maps.get(featureMapId);
    if (!state) {
      return false;
    }
    if (FINISHED.has(state.info.status)) {
      this.maps.delete(featureMapId);
      return true;
    }
    this.stop(state);
    if (state.running === 0) {
      this.finish(state, 'cancelled');
    }
    return true;
  }

  private stop(state: InternalState): void {
    state.cancelRequested = true;
    state.queued -= this.options.scheduler.drop(state);
  }

  private async runBand(state: InternalState, settingsJson: string, firstRow: number, rowCount: number, release: () => void): Promise<void> {
    state.queued -= 1;
    state.running += 1;
    if (state.info.status === 'queued') {
      state.info.status = 'running';
    }
    const { image } = state;
    let failure: string | null = null;
    try {
      const values = await native.computeFeatureMap(state.pixels!, image.width, image.height, image.bitDepth, settingsJson, firstRow, rowCount);
      state.values.set(values, firstRow * state.info.columns);
      state.info.completedRows += rowCount;
    } catch (error) {
      failure = error instanceof Error ? error.message : String(error);
    } finally {
      release();
      state.running -= 1;
    }

    if (FINISHED.has(state.info.status)) {
      return;
    }
    if (failure !== null && state.info.error === null) {
      // One failing band fails the map; the other bands would fail the same way
      state.info.error = failure;
      this.stop(state);
    }
    if (state.running > 0 || state.queued > 0) {
      return;
    }
    if (state.info.error !== null) {
      this.finish(state, 'failed');
    } else {
      this.finish(state, state.cancelRequested ? 'cancelled' : 'completed');
    }
  }

  private finish(state: InternalState, status: FeatureMapStatus): void {
    state.info.status = status;
    state.info.finishedAt = new Date().toISOString();
    state.pixels = null;
  }

  private forgetOldMaps(): void {
    const finished = [...this.maps.values()].filter((state) => FINISHED.has(state.info.status));
    for (const state of finished.slice(0, Math.max(0, finished.length - this.options.retainFinished))) {
      this.maps.delete(state.info.featureMapId);
    }
  }
}
