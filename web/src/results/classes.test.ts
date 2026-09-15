import type { AnalysisSettings, MeasurementResult } from '@glcm/api';
import { describe, expect, it } from 'vitest';
import { keyToAction } from '../viewer/keyboard';
import { classSeriesOf, type Measurement } from './plotData';
import { columnsForRows, rowsForResult } from './rows';

const SETTINGS = { aggregation: 'meanOnly', directions: [0, 45, 90, 135], grayLevels: 32, score: { enabled: false } } as unknown as AnalysisSettings;

function result(roiId: string, roiClass?: string): MeasurementResult {
  return {
    roiId,
    roiName: roiId.toUpperCase(),
    ...(roiClass ? { roiClass } : {}),
    distance: 1,
    status: 'ok',
    error: '',
    pixelCount: 100,
    pairCounts: { '0': 1, '45': 1, '90': 1, '135': 1 },
    quantization: { lower: 0, upper: 255 },
    values: { Contrast: { '0': 1, '45': 2, '90': 3, '135': 4, mean: 2.5, range: 3 } },
    score: null,
    warnings: [],
  };
}

const rows = (...results: MeasurementResult[]) =>
  results.flatMap((entry, index) => rowsForResult(entry, { analysisId: 'ana', index, imageName: 'image.png', settings: SETTINGS }));

describe('classes in results', () => {
  it('adds a Class column only when a row has a class', () => {
    expect(columnsForRows(rows(result('a'), result('b')), []).map((column) => column.id)).not.toContain('class');
    const classified = rows(result('a', 'lesion'), result('b'));
    const columns = columnsForRows(classified, []);
    const classColumn = columns.find((column) => column.id === 'class');
    expect(columns.map((column) => column.id).indexOf('class')).toBe(columns.map((column) => column.id).indexOf('roi') + 1);
    expect(classified.map((row) => classColumn?.value(row))).toEqual(['lesion', '']);
  });

  it('groups plot measurements by class, unclassified ROIs last', () => {
    const measurement = (roiId: string, roiClass: string, mean: number, distance = 1): Measurement => ({
      imageName: 'image.png',
      roiId,
      roiName: roiId,
      roiClass,
      distance,
      values: { '0': mean, '45': mean, '90': mean, '135': mean, mean, range: 0 },
    });
    const series = classSeriesOf([measurement('a', 'normal', 1), measurement('b', '', 2), measurement('c', 'lesion', 3, 2), measurement('d', 'lesion', 4, 1)]);
    expect(series.map((entry) => [entry.label, entry.className, entry.measurements.map((m) => m.roiId)])).toEqual([
      ['lesion', 'lesion', ['d', 'c']],
      ['normal', 'normal', ['a']],
      ['No class', '', ['b']],
    ]);
  });

  it('assigns classes with ⇧ and the number keys', () => {
    const key = (code: string, hasSelection = true) =>
      keyToAction({ key: '!', code, shiftKey: true, ctrlKey: false, metaKey: false, altKey: false }, { hasSelection, view: { width: 100, height: 100 } });
    expect(key('Digit1')).toEqual({ kind: 'assignClass', index: 0 });
    expect(key('Digit9')).toEqual({ kind: 'assignClass', index: 8 });
    expect(key('Digit0')).toEqual({ kind: 'assignClass', index: null });
    expect(key('Digit1', false)).toBeNull();
    // Without ⇧ the 1 key still zooms to 100 %
    expect(keyToAction({ key: '1', code: 'Digit1', shiftKey: false, ctrlKey: false, metaKey: false, altKey: false }, { hasSelection: true, view: { width: 100, height: 100 } })).toEqual({
      kind: 'zoom100',
    });
  });
});
