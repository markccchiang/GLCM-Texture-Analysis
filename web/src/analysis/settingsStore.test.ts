import type { AnalysisSettings } from '@glcm/api';
import { beforeEach, describe, expect, it } from 'vitest';
import { SETTINGS_HISTORY_LIMIT, useAnalysisSettings } from './settingsStore';

const base: AnalysisSettings = {
  features: ['Contrast'],
  grayLevels: 32,
  quantization: { method: 'fixedRange', min: 0, max: 255, binWidth: 8 },
  distances: [1],
  directions: [0, 45, 90, 135],
  aggregation: 'perDirectionAndMean',
  logBase: 'natural',
  score: { enabled: false, age: 40, coefficients: [1, 1, 1, 1], profile: 'calibration', intensityMin: 0, intensityMax: 255 },
};

const store = () => useAnalysisSettings.getState();
const grayLevels = () => store().settings?.grayLevels;

beforeEach(() => {
  useAnalysisSettings.setState({ settings: base, past: [], future: [] });
});

describe('settings undo', () => {
  it('undoes and redoes changes by the user', () => {
    store().update((settings) => ({ ...settings, grayLevels: 64 }));
    store().update((settings) => ({ ...settings, grayLevels: 128 }));
    store().undo();
    expect(grayLevels()).toBe(64);
    store().undo();
    expect(grayLevels()).toBe(32);
    store().undo();
    expect(grayLevels()).toBe(32);
    store().redo();
    expect(grayLevels()).toBe(64);

    // A new change drops the redo steps
    store().update((settings) => ({ ...settings, distances: [1, 2] }));
    expect(store().future).toEqual([]);
    store().redo();
    expect(store().settings?.distances).toEqual([1, 2]);
  });

  it('ignores changes that change nothing', () => {
    store().update((settings) => ({ ...settings }));
    expect(store().past).toEqual([]);
  });

  it('keeps, records or clears the history when settings are replaced', () => {
    store().update((settings) => ({ ...settings, grayLevels: 64 }));

    store().setSettings({ ...base, grayLevels: 16 });
    expect(store().past).toHaveLength(1);

    store().setSettings({ ...base, grayLevels: 8 }, { history: 'record' });
    expect(store().past).toHaveLength(2);
    store().undo();
    expect(grayLevels()).toBe(16);

    store().setSettings(base, { history: 'clear' });
    expect(store().past).toEqual([]);
    expect(store().future).toEqual([]);
  });

  it('limits the history', () => {
    for (let i = 0; i < SETTINGS_HISTORY_LIMIT + 10; i += 1) {
      store().update((settings) => ({ ...settings, distances: [i + 2] }));
    }
    expect(store().past).toHaveLength(SETTINGS_HISTORY_LIMIT);
  });
});
