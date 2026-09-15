// Opening images: upload, then download the raw samples when the server offers them (doc/ui-design-plan.md, 6.1).

import type { ImageInfo, SliceOrientation, VolumeInfo } from '@glcm/api';
import { notifications } from '@mantine/notifications';
import { ApiRequestError, deleteVolume, downloadSample, fetchRawImage, openVolumeSliceImage, uploadImage, uploadVolume } from '../api/client';
import type { RawImage } from '../image/raw';
import { isVolumeFile, needsSliceChoice, useVolumeImport } from '../volumes/volumeImport';
import { useViewer } from './viewerStore';

let currentLoad: AbortController | null = null;

function isAbort(error: unknown): boolean {
  return error instanceof DOMException && error.name === 'AbortError';
}

function errorMessage(error: unknown): string {
  if (error instanceof ApiRequestError || error instanceof Error) {
    return error.message;
  }
  return String(error);
}

function startLoad(): AbortController {
  currentLoad?.abort();
  const controller = new AbortController();
  currentLoad = controller;
  return controller;
}

function finishLoad(controller: AbortController): void {
  if (currentLoad === controller) {
    currentLoad = null;
    useViewer.getState().setLoading(null);
  }
}

export function cancelImageLoad(): void {
  if (currentLoad) {
    const controller = currentLoad;
    controller.abort();
    finishLoad(controller);
  }
}

function progressReporter(controller: AbortController, name: string) {
  return (phase: 'uploading' | 'downloading', progress: number | null) => {
    if (currentLoad === controller) {
      useViewer.getState().setLoading({ name, phase, progress });
    }
  };
}

/** Downloads the raw samples if offered, then shows the image; null if the load was cancelled */
async function showImage(info: ImageInfo, controller: AbortController): Promise<ImageInfo | null> {
  const { signal } = controller;
  const setLoading = progressReporter(controller, info.name);
  let raw: RawImage | null = null;
  if (info.transfer === 'raw') {
    setLoading('downloading', 0);
    try {
      raw = await fetchRawImage(info, (loaded, total) => setLoading('downloading', loaded / total), signal);
    } catch (error) {
      if (isAbort(error)) {
        throw error;
      }
      // display.png and /pixel work for every image, so the image still opens
      console.warn('Raw download failed; using server rendering', error);
      notifications.show({ color: 'yellow', title: 'Using server rendering', message: errorMessage(error) });
    }
  }

  if (signal.aborted) {
    return null;
  }
  useViewer.getState().openImage({ info, raw });
  for (const warning of info.warnings) {
    notifications.show({ color: 'yellow', title: info.name, message: warning, autoClose: 8000 });
  }
  return info;
}

async function uploadAndShow(file: File, controller: AbortController): Promise<ImageInfo | null> {
  const setLoading = progressReporter(controller, file.name);
  setLoading('uploading', 0);
  const info = await uploadImage(file, (loaded, total) => setLoading('uploading', loaded / total), controller.signal);
  return showImage(info, controller);
}

async function run(name: string, load: (controller: AbortController) => Promise<ImageInfo | null>): Promise<ImageInfo | null> {
  const controller = startLoad();
  try {
    return await load(controller);
  } catch (error) {
    if (!isAbort(error)) {
      notifications.show({ color: 'red', title: `Could not open ${name}`, message: errorMessage(error), autoClose: 10000 });
    }
    return null;
  } finally {
    finishLoad(controller);
  }
}

async function sliceAndShow(volume: VolumeInfo, orientation: SliceOrientation, slice: number, volumeIndex: number, controller: AbortController) {
  if (currentLoad === controller) {
    useViewer.getState().setLoading({ name: volume.name, phase: 'openingSlice', progress: null });
  }
  const info = await openVolumeSliceImage(volume.volumeId, { orientation, slice, volume: volumeIndex }, controller.signal);
  return showImage(info, controller);
}

/** Uploads a NIfTI file: a single 2D image opens directly, a volume opens the slice dialog (resolving to null) */
function uploadVolumeAndChoose(file: File): Promise<ImageInfo | null> {
  return run(file.name, async (controller) => {
    const setLoading = progressReporter(controller, file.name);
    setLoading('uploading', 0);
    const volume = await uploadVolume(file, (loaded, total) => setLoading('uploading', loaded / total), controller.signal);
    if (needsSliceChoice(volume)) {
      useVolumeImport.getState().open(volume);
      return null;
    }
    try {
      return await sliceAndShow(volume, volume.acquisitionOrientation, 0, 0, controller);
    } finally {
      void deleteVolume(volume.volumeId).catch(() => undefined);
    }
  });
}

/** Uploads and opens a file; resolves to its info, or null if it failed, was cancelled or waits for a slice to be chosen */
export function openImageFile(file: File): Promise<ImageInfo | null> {
  if (isVolumeFile(file.name)) {
    return uploadVolumeAndChoose(file);
  }
  return run(file.name, (controller) => uploadAndShow(file, controller));
}

/** Opens one slice of an uploaded volume as the image */
export function openVolumeSlice(volume: VolumeInfo, orientation: SliceOrientation, slice: number, volumeIndex: number): Promise<ImageInfo | null> {
  return run(volume.name, (controller) => sliceAndShow(volume, orientation, slice, volumeIndex, controller));
}

/** Opens an image the server already has */
export function openStoredImage(info: ImageInfo): Promise<ImageInfo | null> {
  return run(info.name, (controller) => showImage(info, controller));
}

export function openSample(samplePath: string): Promise<ImageInfo | null> {
  const name = samplePath.split('/').pop() ?? samplePath;
  return run(name, async (controller) => {
    useViewer.getState().setLoading({ name, phase: 'downloadingSample', progress: null });
    const file = await downloadSample(samplePath, controller.signal);
    return uploadAndShow(file, controller);
  });
}
