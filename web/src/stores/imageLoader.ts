// Opening images: upload, then download the raw samples when the server offers them (doc/ui-design-plan.md, 6.1).

import { notifications } from '@mantine/notifications';
import { ApiRequestError, downloadSample, fetchRawImage, uploadImage } from '../api/client';
import type { RawImage } from '../image/raw';
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

async function loadFile(file: File, controller: AbortController): Promise<void> {
  const { signal } = controller;
  const setLoading = (phase: 'uploading' | 'downloading', progress: number | null) => {
    if (currentLoad === controller) {
      useViewer.getState().setLoading({ name: file.name, phase, progress });
    }
  };

  setLoading('uploading', 0);
  const info = await uploadImage(file, (loaded, total) => setLoading('uploading', loaded / total), signal);

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
    return;
  }
  useViewer.getState().openImage({ info, raw });
  for (const warning of info.warnings) {
    notifications.show({ color: 'yellow', title: info.name, message: warning, autoClose: 8000 });
  }
}

export async function openImageFile(file: File): Promise<void> {
  const controller = startLoad();
  try {
    await loadFile(file, controller);
  } catch (error) {
    if (!isAbort(error)) {
      notifications.show({ color: 'red', title: `Could not open ${file.name}`, message: errorMessage(error), autoClose: 10000 });
    }
  } finally {
    finishLoad(controller);
  }
}

export async function openSample(samplePath: string): Promise<void> {
  const controller = startLoad();
  const name = samplePath.split('/').pop() ?? samplePath;
  try {
    useViewer.getState().setLoading({ name, phase: 'downloadingSample', progress: null });
    const file = await downloadSample(samplePath, controller.signal);
    await loadFile(file, controller);
  } catch (error) {
    if (!isAbort(error)) {
      notifications.show({ color: 'red', title: `Could not open ${name}`, message: errorMessage(error), autoClose: 10000 });
    }
  } finally {
    finishLoad(controller);
  }
}
