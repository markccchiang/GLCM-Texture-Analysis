import type { RoiShape } from '@glcm/api';
import { beforeEach, describe, expect, it } from 'vitest';
import { buildRoiSet, prepareRoiImport } from '../files/roiSet';
import { useRois } from './roiStore';

const square: RoiShape = { type: 'rectangle', x: 10, y: 10, width: 20, height: 20 };
const store = useRois.getState;

beforeEach(() => {
  store().reset();
  store().setClasses([]);
});

describe('ROI classes', () => {
  it('adds classes with numbered names and distinct colours', () => {
    const first = store().addClass();
    const second = store().addClass();
    expect([first.name, second.name]).toEqual(['Class 1', 'Class 2']);
    expect(first.color).not.toBe(second.color);
  });

  it('assigns a class and its colour as one undo step', () => {
    const lesion = store().addClass();
    store().updateClass(lesion.name, { name: 'lesion', color: '#FF3B3B' });
    const a = store().addRoi(square);
    const b = store().addRoi(square);
    store().assignClass([a, b], 'lesion');
    expect(store().rois.map((roi) => [roi.className, roi.color])).toEqual([
      ['lesion', '#FF3B3B'],
      ['lesion', '#FF3B3B'],
    ]);
    store().undo();
    expect(store().rois.every((roi) => roi.className === undefined)).toBe(true);
    store().redo();
    store().assignClass([a], null);
    expect(store().rois[0].className).toBeUndefined();
    // Unknown classes are ignored
    store().assignClass([a], 'unknown');
    expect(store().rois[0].className).toBeUndefined();
  });

  it('renames and recolours the ROIs of a class, and removes the class from them', () => {
    store().setClasses([{ name: 'lesion', color: '#FF3B3B' }]);
    const a = store().addRoi(square);
    store().assignClass([a], 'lesion');
    store().updateClass('lesion', { name: '  tumour ', color: '#00C2FF' });
    expect(store().classes).toEqual([{ name: 'tumour', color: '#00C2FF' }]);
    expect(store().rois[0]).toMatchObject({ className: 'tumour', color: '#00C2FF' });

    store().addClass();
    expect(() => store().updateClass('tumour', { name: 'Class 2' })).toThrow(/already/);
    expect(() => store().updateClass('tumour', { name: ' ' })).toThrow(/1 to 100/);

    store().removeClass('tumour');
    expect(store().classes.map((roiClass) => roiClass.name)).toEqual(['Class 2']);
    expect(store().rois[0].className).toBeUndefined();
    expect(store().rois[0].color).toBe('#00C2FF');
  });

  it('keeps classes on duplicates and imports, and merges imported classes', () => {
    store().setClasses([{ name: 'normal', color: '#39FF6A' }]);
    const a = store().addRoi(square);
    store().assignClass([a], 'normal');
    store().duplicateRois([a]);
    expect(store().rois[1]).toMatchObject({ className: 'normal', color: '#39FF6A' });

    store().mergeClasses([
      { name: 'normal', color: '#000000' },
      { name: 'lesion', color: '' },
    ]);
    expect(store().classes.map(({ name }) => name)).toEqual(['normal', 'lesion']);
    expect(store().classes[0].color).toBe('#39FF6A');
    expect(store().classes[1].color).toMatch(/^#[0-9A-F]{6}$/);

    store().importRois([{ name: 'X', color: '', shape: square, className: 'lesion' }]);
    expect(store().rois.at(-1)?.className).toBe('lesion');
  });

  it('writes classes into ROI sets and reads them back', () => {
    const info = { name: 'image.png', width: 100, height: 100, bitDepth: 8, sha256: 'a'.repeat(64) } as never;
    store().setClasses([{ name: 'lesion', color: '#FF3B3B' }]);
    const a = store().addRoi(square);
    store().addRoi(square);
    store().assignClass([a], 'lesion');
    const document = buildRoiSet(info, store().rois, store().classes);
    expect(document.classes).toEqual([{ name: 'lesion', color: '#FF3B3B' }]);
    expect(document.rois[0].class).toBe('lesion');
    expect(document.rois[1]).not.toHaveProperty('class');

    // A class used by an ROI but missing from the list is added with the ROI's colour
    const imported = prepareRoiImport({ ...document, classes: [], rois: [{ ...document.rois[0], class: 'cyst', color: '#123456' }] }, info);
    expect(imported.rois[0].className).toBe('cyst');
    expect(imported.classes).toEqual([{ name: 'cyst', color: '#123456' }]);
    expect(buildRoiSet(info, [])).not.toHaveProperty('classes');
  });
});
