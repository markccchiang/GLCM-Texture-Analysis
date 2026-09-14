// Final results of an analysis whose event stream ended without a "finished" event, e.g. after a dropped connection or
// a proxy timeout. The analysis keeps running on the server, so GET /analyses/{id}/results is polled until its status
// is final.

import type { AnalysisResults } from '@glcm/api';

export interface WaitForResultsOptions {
  /** Delay between requests */
  intervalMs?: number;
  /** Consecutive failed requests tolerated before the last error is thrown */
  maxErrors?: number;
  /** Called with each response of a queued or running analysis, e.g. to show progress */
  onPending?: (results: AnalysisResults) => void;
  sleep?: (ms: number) => Promise<void>;
}

function isFinal(results: AnalysisResults): boolean {
  return results.status !== 'queued' && results.status !== 'running';
}

export async function waitForFinalResults(load: () => Promise<AnalysisResults>, options: WaitForResultsOptions = {}): Promise<AnalysisResults> {
  const { intervalMs = 1000, maxErrors = 5, onPending, sleep = (ms: number) => new Promise<void>((resolve) => setTimeout(resolve, ms)) } = options;
  let errors = 0;
  for (;;) {
    try {
      const results = await load();
      errors = 0;
      if (isFinal(results)) {
        return results;
      }
      onPending?.(results);
    } catch (error) {
      errors += 1;
      if (errors >= maxErrors) {
        throw error;
      }
    }
    await sleep(intervalMs);
  }
}
