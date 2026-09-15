import type { AnalysisSettings, FeatureInfo, FeatureValues, MeasurementResult } from '@glcm/api';
import { describe, expect, it } from 'vitest';
import { boxStats, directionValues, distancesOf, latestMeasurements, niceScale, plotFeatures, SCORE_ID, seriesOf } from './plotData';
import type { AnalysisRun } from './resultsStore';

const values = (v0: number | null, v45: number | null, v90: number | null, v135: number | null, mean: number): FeatureValues => ({ '0': v0, '45': v45, '90': v90, '135': v135, mean, range: 0 });

function result(roiId: string, distance: number, contrast: FeatureValues, extra: Partial<MeasurementResult> = {}): MeasurementResult {
  return {
    roiId,
    roiName: roiId.toUpperCase(),
    distance,
    status: 'ok',
    error: '',
    pixelCount: 100,
    pairCounts: { '0': 1, '45': 1, '90': 1, '135': 1 },
    quantization: { lower: 0, upper: 255 },
    values: { Contrast: contrast },
    score: null,
    warnings: [],
    ...extra,
  };
}

const run = (analysisId: string, imageName: string, results: Array<MeasurementResult | undefined>): AnalysisRun => ({
  analysisId,
  imageName,
  imageSha256: '',
  pixelSpacing: null,
  settings: {} as AnalysisSettings,
  status: 'completed',
  timestamp: '',
  completed: results.length,
  total: results.length,
  results,
});

const features = [
  { id: 'Energy', name: 'Energy' },
  { id: 'Contrast', name: 'Contrast' },
] as FeatureInfo[];

describe('plot data', () => {
  it('lists the features with values in catalog order, and the score when present', () => {
    const runs = [run('a', 'camera.png', [result('r1', 1, values(1, 2, 3, 4, 2.5)), { ...result('r2', 1, values(1, 1, 1, 1, 1)), status: 'failed', values: { Energy: values(1, 1, 1, 1, 1) } }])];
    expect(plotFeatures(runs, features).map((feature) => feature.id)).toEqual(['Contrast']);
    runs.push(run('b', 'camera.png', [result('r1', 1, values(1, 2, 3, 4, 2.5), { score: values(1, 1, 1, 1, 1) })]));
    expect(plotFeatures(runs, features).map((feature) => feature.id)).toEqual(['Contrast', SCORE_ID]);
  });

  it('keeps the latest measurement per image, ROI and distance in first-seen order', () => {
    const runs = [
      run('a', 'camera.png', [result('r1', 1, values(1, 1, 1, 1, 1)), result('r2', 1, values(2, 2, 2, 2, 2)), undefined]),
      run('b', 'camera.png', [result('r1', 1, values(9, 9, 9, 9, 9)), result('r1', 2, values(3, 3, 3, 3, 3))]),
    ];
    const measurements = latestMeasurements(runs, 'Contrast');
    expect(measurements.map((m) => [m.roiId, m.distance, m.values.mean])).toEqual([
      ['r1', 1, 9],
      ['r2', 1, 2],
      ['r1', 2, 3],
    ]);
    expect(distancesOf(measurements)).toEqual([1, 2]);
    expect(latestMeasurements(runs, SCORE_ID)).toEqual([]);
  });

  it('groups series by image and ROI, naming the image when there are several', () => {
    const one = seriesOf(latestMeasurements([run('a', 'camera.png', [result('r1', 2, values(1, 1, 1, 1, 1)), result('r1', 1, values(1, 1, 1, 1, 1))])], 'Contrast'));
    expect(one.map((s) => [s.label, s.measurements.map((m) => m.distance)])).toEqual([['R1', [1, 2]]]);
    const two = seriesOf(
      latestMeasurements([run('a', 'camera.png', [result('r1', 1, values(1, 1, 1, 1, 1))]), run('b', 'brick.png', [result('r1', 1, values(1, 1, 1, 1, 1))])], 'Contrast'),
    );
    expect(two.map((s) => s.label)).toEqual(['camera.png · R1', 'brick.png · R1']);
  });

  it('skips unmeasured directions', () => {
    expect(directionValues(values(1, null, 3, Number.NaN, 2))).toEqual([
      { direction: '0', value: 1 },
      { direction: '90', value: 3 },
    ]);
  });

  it('computes quartiles by linear interpolation', () => {
    expect(boxStats([4, 1, 3, 2])).toEqual({ min: 1, q1: 1.75, median: 2.5, q3: 3.25, max: 4, count: 4 });
    expect(boxStats([5])).toEqual({ min: 5, q1: 5, median: 5, q3: 5, max: 5, count: 1 });
    expect(boxStats([Number.NaN])).toBeNull();
  });

  it('rounds scales out to 1, 2 or 5 steps', () => {
    expect(niceScale([0.13, 0.87])).toEqual({ min: 0, max: 1, ticks: [0, 0.2, 0.4, 0.6, 0.8, 1] });
    expect(niceScale([12, 47], { includeZero: true })).toEqual({ min: 0, max: 50, ticks: [0, 10, 20, 30, 40, 50] });
    // A range of 1.2 in 5 steps needs steps of at least 0.24: the next nice step is 0.5
    expect(niceScale([-0.3, 0.9]).ticks).toEqual([-0.5, 0, 0.5, 1]);
    expect(niceScale([-0.3, 0.7]).ticks).toEqual([-0.4, -0.2, 0, 0.2, 0.4, 0.6, 0.8]);
    // A single value still gets a range
    expect(niceScale([5]).min).toBeLessThan(5);
    expect(niceScale([]).ticks.length).toBeGreaterThan(1);
  });
});
