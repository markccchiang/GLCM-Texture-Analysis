// Retention (doc/ui-design-plan.md, section 8.2): uploaded images and results expire after a configurable time.

import type { FastifyBaseLogger } from 'fastify';
import type { JobManager } from '../analysis/JobManager.js';
import type { ImageStore } from './ImageStore.js';
import type { ResultStore } from './ResultStore.js';

export interface RetentionTargets {
  images: ImageStore;
  results: ResultStore;
  jobs: JobManager;
}

export interface PurgeSummary {
  images: number;
  analyses: number;
}

/** Deletes images uploaded and analyses finished more than maxAgeMs before now */
export async function purgeExpired(targets: RetentionTargets, maxAgeMs: number, now = Date.now()): Promise<PurgeSummary> {
  const cutoff = now - maxAgeMs;
  let images = 0;
  for (const info of await targets.images.list()) {
    if (Date.parse(info.createdAt) < cutoff && (await targets.images.remove(info.imageId))) {
      images += 1;
    }
  }
  targets.jobs.forgetFinishedBefore(cutoff);
  const analyses = await targets.results.removeFinishedBefore(cutoff);
  return { images, analyses };
}

/** Purges now and then periodically (at most hourly); returns a function that stops it */
export function startRetention(targets: RetentionTargets, maxAgeMs: number, log: FastifyBaseLogger): () => void {
  const run = async () => {
    try {
      const summary = await purgeExpired(targets, maxAgeMs);
      if (summary.images > 0 || summary.analyses > 0) {
        log.info(summary, 'Removed expired images and analyses');
      }
    } catch (error) {
      log.error(error, 'Retention cleanup failed');
    }
  };
  const initial = run();
  const timer = setInterval(() => void run(), Math.min(60 * 60_000, Math.max(60_000, maxAgeMs / 4)));
  timer.unref();
  return () => {
    clearInterval(timer);
    void initial;
  };
}
