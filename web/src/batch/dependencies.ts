// The API calls and results table used by a batch in the browser

import { cancelAnalysis, findImagesBySha256, getAnalysisResults, startAnalysis, uploadImage } from '../api/client';
import { waitForFinalResults } from '../analysis/waitForResults';
import { useResults } from '../results/resultsStore';
import type { BatchDependencies } from './runBatch';
import { fileSha256 } from './sha256';
import { usePreferences } from '../stores/preferences';

export function browserBatchDependencies(): BatchDependencies {
  return {
    sha256: fileSha256,
    findImagesBySha256,
    uploadImage: (file, onProgress, signal) => uploadImage(file, onProgress, signal),
    startAnalysis,
    waitForResults: (analysisId, onProgress) =>
      waitForFinalResults(() => getAnalysisResults(analysisId), {
        intervalMs: 500,
        onPending: (partial) => {
          const run = useResults.getState().runs.find((candidate) => candidate.analysisId === analysisId);
          useResults.getState().setProgress(analysisId, partial.results.length, run?.total ?? partial.results.length);
          onProgress(partial.results.length);
        },
      }),
    cancelAnalysis,
    pixelSpacing: (info) => usePreferences.getState().pixelSpacings[info.sha256],
    // Each image appears in the Results table like a measurement of the open image
    onAnalysisStarted: (info) => useResults.getState().startRun(info),
    onAnalysisFinished: (results) => useResults.getState().finishRun(results.analysisId, results.status, results.results, results.timestamp),
  };
}
