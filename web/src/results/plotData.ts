// Data for the Results plots: the latest value of one feature per image, ROI and distance, and the scales and statistics
// the charts are drawn with. The charts use the per-direction values of each measurement, whatever the aggregation of
// the table rows.

import type { FeatureInfo, FeatureValues, MeasurementResult } from '@glcm/api';
import type { AnalysisRun } from './resultsStore';

/** The score, plotted like a feature */
export const SCORE_ID = 'score';
export const DIRECTIONS = ['0', '45', '90', '135'] as const;
export type Direction = (typeof DIRECTIONS)[number];

export interface PlotFeature {
  id: string;
  name: string;
}

export interface Measurement {
  imageName: string;
  roiId: string;
  roiName: string;
  /** Empty when the ROI has no class */
  roiClass: string;
  distance: number;
  values: FeatureValues;
}

/** One line, bar or box: an ROI on one image, or every ROI of a class */
export interface Series {
  key: string;
  label: string;
  imageName: string;
  roiId: string;
  /** Set for the series of a class ('' for ROIs without a class) */
  className?: string;
  /** Sorted by distance */
  measurements: Measurement[];
}

export function isFiniteNumber(value: number | null | undefined): value is number {
  return typeof value === 'number' && Number.isFinite(value);
}

function okResults(runs: readonly AnalysisRun[]): MeasurementResult[] {
  return runs.flatMap((run) => run.results.filter((result): result is MeasurementResult => result?.status === 'ok'));
}

/** Features with values in any measurement, in catalog order, then the score when a measurement has one */
export function plotFeatures(runs: readonly AnalysisRun[], features: readonly FeatureInfo[]): PlotFeature[] {
  const results = okResults(runs);
  const present = new Set(results.flatMap((result) => Object.keys(result.values)));
  const list = features.filter((feature) => present.has(feature.id)).map(({ id, name }) => ({ id, name }));
  if (results.some((result) => result.score)) {
    list.push({ id: SCORE_ID, name: 'Score' });
  }
  return list;
}

/** One measurement per image, ROI and distance: a later measurement replaces an earlier one but keeps its position */
export function latestMeasurements(runs: readonly AnalysisRun[], featureId: string): Measurement[] {
  const measurements = new Map<string, Measurement>();
  for (const run of runs) {
    for (const result of run.results) {
      if (result?.status !== 'ok') {
        continue;
      }
      const values = featureId === SCORE_ID ? result.score : result.values[featureId];
      if (!values) {
        continue;
      }
      measurements.set(JSON.stringify([run.imageName, result.roiId, result.distance]), {
        imageName: run.imageName,
        roiId: result.roiId,
        roiName: result.roiName,
        roiClass: result.roiClass ?? '',
        distance: result.distance,
        values,
      });
    }
  }
  return [...measurements.values()];
}

/** Series by image and ROI, labelled with the image name once measurements come from several images */
export function seriesOf(measurements: readonly Measurement[]): Series[] {
  const severalImages = new Set(measurements.map((measurement) => measurement.imageName)).size > 1;
  const series = new Map<string, Series>();
  for (const measurement of measurements) {
    const key = JSON.stringify([measurement.imageName, measurement.roiId]);
    const label = severalImages ? `${measurement.imageName} · ${measurement.roiName}` : measurement.roiName;
    const existing = series.get(key);
    if (existing) {
      existing.measurements.push(measurement);
      existing.label = label;
    } else {
      series.set(key, { key, label, imageName: measurement.imageName, roiId: measurement.roiId, measurements: [measurement] });
    }
  }
  return [...series.values()].map((entry) => ({ ...entry, measurements: [...entry.measurements].sort((a, b) => a.distance - b.distance) }));
}

/** One series per class with the measurements of all its ROIs (on every image), classes in name order and ROIs without a class last */
export function classSeriesOf(measurements: readonly Measurement[]): Series[] {
  const series = new Map<string, Series>();
  for (const measurement of measurements) {
    const existing = series.get(measurement.roiClass);
    if (existing) {
      existing.measurements.push(measurement);
    } else {
      series.set(measurement.roiClass, {
        key: `class:${measurement.roiClass}`,
        label: measurement.roiClass || 'No class',
        imageName: '',
        roiId: '',
        className: measurement.roiClass,
        measurements: [measurement],
      });
    }
  }
  return [...series.values()]
    .sort((a, b) => (a.className === '' ? 1 : b.className === '' ? -1 : a.label.localeCompare(b.label)))
    .map((entry) => ({ ...entry, measurements: [...entry.measurements].sort((a, b) => a.distance - b.distance) }));
}

export function distancesOf(measurements: readonly Measurement[]): number[] {
  return [...new Set(measurements.map((measurement) => measurement.distance))].sort((a, b) => a - b);
}

/** The values of the measured directions (unselected directions are null) */
export function directionValues(values: FeatureValues): Array<{ direction: Direction; value: number }> {
  return DIRECTIONS.flatMap((direction) => {
    const value = values[direction];
    return isFiniteNumber(value) ? [{ direction, value }] : [];
  });
}

export interface BoxStats {
  min: number;
  q1: number;
  median: number;
  q3: number;
  max: number;
  count: number;
}

/** Quantile of sorted values with linear interpolation between order statistics (R type 7, NumPy's default) */
function quantile(sorted: readonly number[], p: number): number {
  const position = (sorted.length - 1) * p;
  const lower = Math.floor(position);
  const upper = Math.min(lower + 1, sorted.length - 1);
  return sorted[lower] + (position - lower) * (sorted[upper] - sorted[lower]);
}

export function boxStats(values: readonly number[]): BoxStats | null {
  const sorted = values.filter(isFiniteNumber).sort((a, b) => a - b);
  if (sorted.length === 0) {
    return null;
  }
  return {
    min: sorted[0],
    q1: quantile(sorted, 0.25),
    median: quantile(sorted, 0.5),
    q3: quantile(sorted, 0.75),
    max: sorted[sorted.length - 1],
    count: sorted.length,
  };
}

export interface Scale {
  min: number;
  max: number;
  ticks: number[];
}

const tidy = (value: number) => Number(value.toPrecision(12));

/** A domain covering the values, rounded out to ticks at 1, 2 or 5 × 10^k */
export function niceScale(values: readonly number[], { includeZero = false, count = 5 } = {}): Scale {
  const data = values.filter(isFiniteNumber);
  let low = data.length > 0 ? data.reduce((a, b) => Math.min(a, b)) : 0;
  let high = data.length > 0 ? data.reduce((a, b) => Math.max(a, b)) : 1;
  if (includeZero) {
    low = Math.min(low, 0);
    high = Math.max(high, 0);
  }
  if (low === high) {
    const pad = low === 0 ? 1 : Math.abs(low) * 0.1;
    low -= pad;
    high += pad;
  }
  const raw = (high - low) / count;
  const power = 10 ** Math.floor(Math.log10(raw));
  const step = [1, 2, 5, 10].map((multiple) => multiple * power).find((candidate) => candidate >= raw * (1 - 1e-9))!;
  const min = Math.floor(tidy(low / step)) * step;
  const max = Math.ceil(tidy(high / step)) * step;
  const ticks: number[] = [];
  for (let i = 0; min + i * step <= max + step * 1e-9; i += 1) {
    ticks.push(tidy(min + i * step));
  }
  return { min: tidy(min), max: tidy(max), ticks };
}
