import type { AnalysisResults, AnalysisStatus } from '@glcm/api';
import { describe, expect, it, vi } from 'vitest';
import { waitForFinalResults } from './waitForResults';

const results = (status: AnalysisStatus, count = 0) =>
  ({ status, results: Array.from({ length: count }, (_, i) => ({ roiId: `r${i}` })) }) as unknown as AnalysisResults;

/** A loader returning the given responses in order; Error values are thrown */
function loader(responses: Array<AnalysisResults | Error>) {
  const load = vi.fn(async () => {
    const next = responses.shift();
    if (!next) {
      throw new Error('no more responses');
    }
    if (next instanceof Error) {
      throw next;
    }
    return next;
  });
  return load;
}

describe('waitForFinalResults', () => {
  it('returns finished results at once', async () => {
    const load = loader([results('completed', 3)]);
    const sleep = vi.fn(async () => {});
    expect(await waitForFinalResults(load, { sleep })).toMatchObject({ status: 'completed' });
    expect(load).toHaveBeenCalledTimes(1);
    expect(sleep).not.toHaveBeenCalled();
  });

  it('polls while the analysis is queued or running and reports progress', async () => {
    const load = loader([results('queued'), results('running', 1), results('running', 2), results('cancelled', 2)]);
    const sleep = vi.fn(async (_ms: number) => {});
    const onPending = vi.fn();
    const final = await waitForFinalResults(load, { sleep, onPending, intervalMs: 250 });
    expect(final.status).toBe('cancelled');
    expect(load).toHaveBeenCalledTimes(4);
    expect(onPending.mock.calls.map(([pending]) => (pending as AnalysisResults).results.length)).toEqual([0, 1, 2]);
    expect(sleep.mock.calls.map(([ms]) => ms)).toEqual([250, 250, 250]);
  });

  it('tolerates a few failed requests in a row', async () => {
    const load = loader([new Error('offline'), results('running'), new Error('offline'), new Error('offline'), results('completed')]);
    const final = await waitForFinalResults(load, { sleep: async () => {}, maxErrors: 3 });
    expect(final.status).toBe('completed');
    expect(load).toHaveBeenCalledTimes(5);
  });

  it('gives up after too many failed requests in a row', async () => {
    const load = loader([results('running'), new Error('first'), new Error('second'), new Error('third')]);
    await expect(waitForFinalResults(load, { sleep: async () => {}, maxErrors: 3 })).rejects.toThrow('third');
  });
});
