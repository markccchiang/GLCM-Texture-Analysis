import { randomUUID } from 'node:crypto';
import fs from 'node:fs/promises';
import path from 'node:path';
import type { AnalysisInfo, AnalysisResults } from '@glcm/api';

const ID_PATTERN = /^ana_[0-9a-f]{32}$/;

export interface StoredAnalysis {
  info: AnalysisInfo;
  results: AnalysisResults;
}

function isNotFound(error: unknown): boolean {
  return (error as NodeJS.ErrnoException | undefined)?.code === 'ENOENT';
}

/** Finished analyses on disk as results/<analysisId>.json, so their results survive restarts */
export class ResultStore {
  readonly resultsDir: string;

  constructor(dataDir: string) {
    this.resultsDir = path.join(dataDir, 'results');
  }

  async init(): Promise<void> {
    await fs.mkdir(this.resultsDir, { recursive: true });
  }

  private file(id: string): string {
    if (!ID_PATTERN.test(id)) {
      throw new Error(`Invalid analysis id: ${id}`);
    }
    return path.join(this.resultsDir, `${id}.json`);
  }

  async save(analysis: StoredAnalysis): Promise<void> {
    const target = this.file(analysis.info.analysisId);
    const temporary = `${target}.${randomUUID()}.tmp`;
    await fs.writeFile(temporary, JSON.stringify(analysis));
    await fs.rename(temporary, target);
  }

  async load(id: string): Promise<StoredAnalysis | undefined> {
    if (!ID_PATTERN.test(id)) {
      return undefined;
    }
    try {
      return JSON.parse(await fs.readFile(this.file(id), 'utf8')) as StoredAnalysis;
    } catch (error) {
      if (isNotFound(error)) {
        return undefined;
      }
      throw error;
    }
  }

  /** Deletes analyses that finished before a time (milliseconds since the epoch); returns how many */
  async removeFinishedBefore(cutoff: number): Promise<number> {
    let removed = 0;
    for (const name of await fs.readdir(this.resultsDir)) {
      const id = name.replace(/\.json$/, '');
      if (!ID_PATTERN.test(id) || name !== `${id}.json`) {
        continue;
      }
      const stored = await this.load(id).catch(() => undefined);
      const finished = stored ? Date.parse(stored.info.finishedAt ?? stored.info.createdAt) : NaN;
      if (!stored || finished < cutoff) {
        await fs.rm(this.file(id), { force: true });
        removed += 1;
      }
    }
    return removed;
  }
}
