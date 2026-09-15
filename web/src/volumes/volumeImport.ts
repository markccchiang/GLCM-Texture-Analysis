// Choosing the slice of a NIfTI volume to open as the image (File ▸ Open of a .nii or .nii.gz file)

import type { SliceOrientation, VolumeInfo } from '@glcm/api';
import { create } from 'zustand';

export const ORIENTATIONS: SliceOrientation[] = ['axial', 'coronal', 'sagittal'];

export const ORIENTATION_LABELS: Record<SliceOrientation, string> = { axial: 'Axial', coronal: 'Coronal', sagittal: 'Sagittal' };

/** NIfTI files go to POST /volumes; everything else is uploaded as an image */
export function isVolumeFile(name: string): boolean {
  return /\.nii(\.gz)?$/i.test(name);
}

/** A 2D file (one slice in its own plane and one volume) opens without asking */
export function needsSliceChoice(volume: VolumeInfo): boolean {
  return volume.volumes > 1 || volume.slices[volume.acquisitionOrientation].count > 1;
}

export function middleSlice(count: number): number {
  return Math.max(0, Math.floor((count - 1) / 2));
}

function formatMm(value: number): string {
  return String(Number(value.toPrecision(3)));
}

/** "256 × 256 × 170 voxels · 4 volumes · int16 · RAS" */
export function volumeSummary(volume: VolumeInfo): string {
  const parts = [`${volume.dimensions.join(' × ')} voxels`];
  if (volume.volumes > 1) {
    parts.push(`${volume.volumes} volumes`);
  }
  parts.push(volume.dataType, volume.orientationSource === 'none' ? 'no orientation' : volume.axisCodes);
  return parts.join(' · ');
}

/** "256 × 170 px · 1 × 1.2 mm" */
export function sliceSummary(volume: VolumeInfo, orientation: SliceOrientation): string {
  const { width, height, pixelSpacing } = volume.slices[orientation];
  const size = `${width} × ${height} px`;
  return pixelSpacing ? `${size} · ${formatMm(pixelSpacing.x)} × ${formatMm(pixelSpacing.y)} mm` : size;
}

/** Width and height of the preview, fitting the slice's physical proportions into a square of the given size */
export function previewSize(volume: VolumeInfo, orientation: SliceOrientation, size: number): { width: number; height: number } {
  const { width, height, pixelSpacing } = volume.slices[orientation];
  const ratio = (width * (pixelSpacing?.x ?? 1)) / (height * (pixelSpacing?.y ?? 1));
  return ratio >= 1 ? { width: size, height: Math.max(1, Math.round(size / ratio)) } : { width: Math.max(1, Math.round(size * ratio)), height: size };
}

interface VolumeImportState {
  volume: VolumeInfo | null;
  orientation: SliceOrientation;
  /** The slice chosen in each orientation, so switching back keeps it */
  slices: Record<SliceOrientation, number>;
  volumeIndex: number;
  open(volume: VolumeInfo): void;
  setOrientation(orientation: SliceOrientation): void;
  setSlice(slice: number): void;
  setVolumeIndex(volumeIndex: number): void;
  close(): void;
}

const clamp = (value: number, count: number) => Math.min(Math.max(0, Math.round(value)), Math.max(0, count - 1));

export const useVolumeImport = create<VolumeImportState>()((set, get) => ({
  volume: null,
  orientation: 'axial',
  slices: { axial: 0, coronal: 0, sagittal: 0 },
  volumeIndex: 0,
  open: (volume) =>
    set({
      volume,
      orientation: volume.acquisitionOrientation,
      slices: {
        axial: middleSlice(volume.slices.axial.count),
        coronal: middleSlice(volume.slices.coronal.count),
        sagittal: middleSlice(volume.slices.sagittal.count),
      },
      volumeIndex: 0,
    }),
  setOrientation: (orientation) => set({ orientation }),
  setSlice: (slice) => {
    const { volume, orientation, slices } = get();
    if (volume) {
      set({ slices: { ...slices, [orientation]: clamp(slice, volume.slices[orientation].count) } });
    }
  },
  setVolumeIndex: (volumeIndex) => {
    const { volume } = get();
    if (volume) {
      set({ volumeIndex: clamp(volumeIndex, volume.volumes) });
    }
  },
  close: () => set({ volume: null }),
}));
