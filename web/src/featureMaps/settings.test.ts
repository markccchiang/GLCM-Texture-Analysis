import type { AnalysisSettings, CatalogResponse } from '@glcm/api';
import { describe, expect, it } from 'vitest';
import { automaticStep, buildFeatureMapSettings, choiceProblem, describeQuantization, mapGrid, mappableFeatures } from './settings';

const ANALYSIS: AnalysisSettings = {
  features: ['Contrast'],
  grayLevels: 64,
  quantization: { method: 'fixedRange', min: 0, max: 4095, binWidth: 1 },
  distances: [2, 1],
  directions: [90, 0],
  aggregation: 'meanOnly',
  logBase: 'log2',
  score: { enabled: false, age: 40, coefficients: [0, 0, 0, 0], profile: 'calibration', intensityMin: 0, intensityMax: 255 },
};

describe('feature map settings', () => {
  it('chooses a step that keeps at most 512 points per side', () => {
    expect(automaticStep(512, 100)).toBe(1);
    expect(automaticStep(513, 10)).toBe(2);
    expect(mapGrid(1025, 300, null)).toEqual({ step: 3, columns: 342, rows: 100 });
    expect(mapGrid(10, 7, 4)).toEqual({ step: 4, columns: 3, rows: 2 });
  });

  it('offers the co-occurrence features except the Maximal Correlation Coefficient', () => {
    const feature = (id: string, group: string) => ({ id, name: id, group, nonStandard: false, nonStandardReason: '', docAnchor: '', cost: 'normal' });
    const catalog = {
      features: [feature('Mean', 'regionStatistics'), feature('Contrast', 'haralick'), feature('MaximalCorrelationCoefficient', 'haralick'), feature('ClusterShade', 'other'), feature('GlrlmRunEntropy', 'runLength')],
    } as unknown as CatalogResponse;
    expect(mappableFeatures(catalog).map((info) => info.id)).toEqual(['Contrast', 'ClusterShade']);
  });

  it('describes the first problem of a choice', () => {
    const choice = { feature: 'Contrast', window: 15, step: null, distance: 1 };
    expect(choiceProblem(choice, 512, 512)).toBeNull();
    expect(choiceProblem({ ...choice, window: 14 }, 512, 512)).toMatch(/odd number/);
    expect(choiceProblem({ ...choice, window: 129 }, 512, 512)).toMatch(/between 3 and 127/);
    expect(choiceProblem({ ...choice, window: 3, distance: 3 }, 512, 512)).toMatch(/smaller than the window/);
    expect(choiceProblem({ ...choice, step: 0 }, 512, 512)).toMatch(/whole number/);
    expect(choiceProblem({ ...choice, step: 2 }, 5000, 100)).toMatch(/use at least 3/);
    expect(choiceProblem({ ...choice, step: 3 }, 5000, 100)).toBeNull();
  });

  it('takes gray levels, quantization, directions and log base from the analysis settings', () => {
    expect(buildFeatureMapSettings(ANALYSIS, { feature: 'Entropy', window: 9, step: null, distance: 2 })).toEqual({
      feature: 'Entropy',
      window: 9,
      step: null,
      grayLevels: 64,
      quantization: ANALYSIS.quantization,
      distance: 2,
      directions: [0, 90],
      logBase: 'log2',
    });
    expect(describeQuantization(ANALYSIS.quantization)).toBe('fixed range 0–4095');
    expect(describeQuantization({ ...ANALYSIS.quantization, method: 'fixedBinWidth', binWidth: 8 })).toBe('bin width 8 from the image minimum');
  });
});
