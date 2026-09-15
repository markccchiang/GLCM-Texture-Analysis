import type { VolumeInfo } from '@glcm/api';
import { beforeEach, describe, expect, it } from 'vitest';
import { isVolumeFile, middleSlice, needsSliceChoice, previewSize, sliceSummary, useVolumeImport, volumeSummary } from './volumeImport';

const VOLUME: VolumeInfo = {
  volumeId: `vol_${'a'.repeat(32)}`,
  name: 'brain.nii.gz',
  sizeBytes: 1000,
  niftiVersion: 1,
  dimensions: [256, 256, 170],
  volumes: 1,
  dataType: 'int16',
  axisCodes: 'LAS',
  orientationSource: 'sform',
  acquisitionOrientation: 'axial',
  slices: {
    axial: { count: 170, width: 256, height: 256, pixelSpacing: { x: 1, y: 1 } },
    coronal: { count: 256, width: 256, height: 170, pixelSpacing: { x: 1, y: 1.2 } },
    sagittal: { count: 256, width: 256, height: 170, pixelSpacing: null },
  },
  minimum: 0,
  maximum: 900,
  bitDepth: 16,
  valueConversion: null,
  windowMin: 0,
  windowMax: 800,
  warnings: [],
  createdAt: '2026-09-15T10:00:00Z',
};

const store = useVolumeImport.getState;

beforeEach(() => store().close());

describe('volume import', () => {
  it('recognizes NIfTI files by name', () => {
    expect(['brain.nii', 'BRAIN.NII.GZ', 'a.b.nii.gz'].map(isVolumeFile)).toEqual([true, true, true]);
    expect(['brain.gz', 'scan.dcm', 'nii.png', 'brain.nii.zip'].map(isVolumeFile)).toEqual([false, false, false, false]);
  });

  it('asks for a slice only for volumes', () => {
    expect(needsSliceChoice(VOLUME)).toBe(true);
    const flat = { ...VOLUME, dimensions: [256, 256, 1], slices: { ...VOLUME.slices, axial: { ...VOLUME.slices.axial, count: 1 } } } as VolumeInfo;
    expect(needsSliceChoice(flat)).toBe(false);
    expect(needsSliceChoice({ ...flat, volumes: 3 })).toBe(true);
  });

  it('describes the volume and its slices', () => {
    expect(volumeSummary(VOLUME)).toBe('256 × 256 × 170 voxels · int16 · LAS');
    expect(volumeSummary({ ...VOLUME, volumes: 4, orientationSource: 'none' })).toBe('256 × 256 × 170 voxels · 4 volumes · int16 · no orientation');
    expect(sliceSummary(VOLUME, 'coronal')).toBe('256 × 170 px · 1 × 1.2 mm');
    expect(sliceSummary(VOLUME, 'sagittal')).toBe('256 × 170 px');
    // 256 mm wide, 204 mm high
    expect(previewSize(VOLUME, 'coronal', 320)).toEqual({ width: 320, height: 255 });
    expect(previewSize({ ...VOLUME, slices: { ...VOLUME.slices, axial: { count: 1, width: 10, height: 40, pixelSpacing: null } } }, 'axial', 320)).toEqual({
      width: 80,
      height: 320,
    });
  });

  it('starts in the acquisition plane at the middle slices and keeps a slice per orientation', () => {
    expect([middleSlice(170), middleSlice(1), middleSlice(4)]).toEqual([84, 0, 1]);
    store().open({ ...VOLUME, volumes: 3, acquisitionOrientation: 'coronal' });
    expect(store()).toMatchObject({ orientation: 'coronal', slices: { axial: 84, coronal: 127, sagittal: 127 }, volumeIndex: 0 });

    store().setSlice(300);
    expect(store().slices.coronal).toBe(255);
    store().setOrientation('axial');
    store().setSlice(-4);
    expect(store().slices).toEqual({ axial: 0, coronal: 255, sagittal: 127 });
    store().setVolumeIndex(7);
    expect(store().volumeIndex).toBe(2);

    store().close();
    expect(store().volume).toBeNull();
  });
});
