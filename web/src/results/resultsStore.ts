// Rows of the Results table and the analyses producing them (doc/ui-design-plan.md, section 6.3.3). Rows are appended
// and keep their settings; changing settings never alters them.

import type { AnalysisInfo, AnalysisSettings, AnalysisStatus, MeasurementResult, PixelSpacing } from '@glcm/api';
import { create } from 'zustand';
import { rowsForResult, type ResultRow } from './rows';

export interface AnalysisRun {
  analysisId: string;
  imageName: string;
  imageSha256: string;
  settings: AnalysisSettings;
  /** Spacing the analysis was started with, for ROI areas */
  pixelSpacing: PixelSpacing | null;
  status: AnalysisStatus;
  /** When the analysis finished (or started, while it runs) */
  timestamp: string;
  completed: number;
  total: number;
  /** Finished jobs by index */
  results: Array<MeasurementResult | undefined>;
}

export interface ResultsState {
  runs: AnalysisRun[];
  rows: ResultRow[];
  hiddenColumns: string[];
  startRun(info: AnalysisInfo): void;
  addResult(analysisId: string, index: number, result: MeasurementResult): void;
  setProgress(analysisId: string, completed: number, total: number): void;
  /** Marks a run finished; `results` (ordered, from GET /results) replaces the streamed ones when given */
  finishRun(analysisId: string, status: AnalysisStatus, results?: MeasurementResult[], timestamp?: string): void;
  /** Replaces the table with stored runs (an opened project) */
  loadRuns(runs: AnalysisRun[]): void;
  clear(): void;
  toggleColumn(columnId: string): void;
}

function buildRows(runs: readonly AnalysisRun[]): ResultRow[] {
  return runs.flatMap((run) =>
    run.results.flatMap((result, index) =>
      result ? rowsForResult(result, { analysisId: run.analysisId, index, imageName: run.imageName, settings: run.settings, pixelSpacing: run.pixelSpacing }) : [],
    ),
  );
}

export function isRunning(run: AnalysisRun): boolean {
  return run.status === 'queued' || run.status === 'running';
}

export const useResults = create<ResultsState>()((set, get) => {
  const updateRun = (analysisId: string, change: (run: AnalysisRun) => AnalysisRun) => {
    const runs = get().runs.map((run) => (run.analysisId === analysisId ? change(run) : run));
    set({ runs, rows: buildRows(runs) });
  };

  return {
    runs: [],
    rows: [],
    hiddenColumns: [],

    startRun: (info) => {
      const run: AnalysisRun = {
        analysisId: info.analysisId,
        imageName: info.imageName,
        imageSha256: info.imageSha256,
        settings: info.settings,
        pixelSpacing: info.pixelSpacing ?? null,
        status: info.status,
        timestamp: info.createdAt,
        completed: info.completed,
        total: info.total,
        results: new Array<MeasurementResult | undefined>(info.total),
      };
      set({ runs: [...get().runs, run] });
    },

    addResult: (analysisId, index, result) =>
      updateRun(analysisId, (run) => {
        const results = [...run.results];
        results[index] = result;
        return { ...run, results };
      }),

    setProgress: (analysisId, completed, total) =>
      set({ runs: get().runs.map((run) => (run.analysisId === analysisId ? { ...run, completed, total, status: 'running' } : run)) }),

    finishRun: (analysisId, status, results, timestamp) =>
      updateRun(analysisId, (run) => ({ ...run, status, results: results ?? run.results, timestamp: timestamp ?? run.timestamp })),

    loadRuns: (runs) => set({ runs, rows: buildRows(runs) }),

    // Running analyses keep their entry so their remaining results still arrive
    clear: () => {
      const runs = get()
        .runs.filter(isRunning)
        .map((run) => ({ ...run, results: new Array<MeasurementResult | undefined>(run.total) }));
      set({ runs, rows: [] });
    },

    toggleColumn: (columnId) => {
      const { hiddenColumns } = get();
      set({ hiddenColumns: hiddenColumns.includes(columnId) ? hiddenColumns.filter((id) => id !== columnId) : [...hiddenColumns, columnId] });
    },
  };
});
