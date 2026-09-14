// HTTP client of the /api/v1 API (doc/ui-design-plan.md, section 8.5).

import {
  API_PREFIX,
  type AnalysisInfo,
  type AnalysisRequest,
  type AnalysisResults,
  type RoiStatsRequest,
  type RoiStatsResponse,
  type CatalogResponse,
  type ExportFormat,
  type HealthResponse,
  type ImageListResponse,
  type ResultsDocument,
  type RoiImagesExportRequest,
  type ErrorResponse,
  type ImageInfo,
  type PixelResponse,
  type SamplesResponse,
} from '@glcm/api';
import { fileNameFromDisposition } from '../files/download';
import { decodeRawSamples, rawFormatFromHeaders, type RawImage } from '../image/raw';

export class ApiRequestError extends Error {
  constructor(
    readonly status: number,
    readonly code: string,
    message: string,
  ) {
    super(message);
    this.name = 'ApiRequestError';
  }
}

export type ProgressCallback = (loaded: number, total: number) => void;

async function errorFromResponse(response: Response): Promise<ApiRequestError> {
  try {
    const body = (await response.json()) as Partial<ErrorResponse>;
    return new ApiRequestError(response.status, body.error ?? 'HttpError', body.message ?? response.statusText);
  } catch {
    return new ApiRequestError(response.status, 'HttpError', `${response.status} ${response.statusText}`);
  }
}

async function getJson<T>(url: string, signal?: AbortSignal): Promise<T> {
  const response = await fetch(url, { signal, headers: { accept: 'application/json' } });
  if (!response.ok) {
    throw await errorFromResponse(response);
  }
  return (await response.json()) as T;
}

export function getCatalog(signal?: AbortSignal): Promise<CatalogResponse> {
  return getJson(`${API_PREFIX}/catalog`, signal);
}

export function getSamples(signal?: AbortSignal): Promise<SamplesResponse> {
  return getJson(`${API_PREFIX}/samples`, signal);
}

export async function downloadSample(samplePath: string, signal?: AbortSignal): Promise<File> {
  const response = await fetch(`${API_PREFIX}/samples/file?path=${encodeURIComponent(samplePath)}`, { signal });
  if (!response.ok) {
    throw await errorFromResponse(response);
  }
  const blob = await response.blob();
  return new File([blob], samplePath.split('/').pop() ?? samplePath, { type: blob.type });
}

export function getImageInfo(imageId: string, signal?: AbortSignal): Promise<ImageInfo> {
  return getJson(`${API_PREFIX}/images/${imageId}`, signal);
}

/** Uploads with XMLHttpRequest, which (unlike fetch) reports upload progress */
export function uploadImage(file: File, onProgress?: ProgressCallback, signal?: AbortSignal): Promise<ImageInfo> {
  return new Promise((resolve, reject) => {
    const request = new XMLHttpRequest();
    request.open('POST', `${API_PREFIX}/images`);
    request.responseType = 'json';
    request.setRequestHeader('accept', 'application/json');

    request.upload.onprogress = (event) => {
      if (event.lengthComputable) {
        onProgress?.(event.loaded, event.total);
      }
    };
    request.onload = () => {
      const body = request.response as (ImageInfo & Partial<ErrorResponse>) | null;
      if (request.status === 201 && body) {
        resolve(body);
      } else {
        reject(new ApiRequestError(request.status, body?.error ?? 'HttpError', body?.message ?? `Upload failed with status ${request.status}`));
      }
    };
    request.onerror = () => reject(new ApiRequestError(0, 'NetworkError', 'The server could not be reached'));
    request.onabort = () => reject(new DOMException('The upload was cancelled', 'AbortError'));
    signal?.addEventListener('abort', () => request.abort(), { once: true });

    const form = new FormData();
    form.append('file', file, file.name);
    request.send(form);
  });
}

export async function deleteImage(imageId: string): Promise<void> {
  const response = await fetch(`${API_PREFIX}/images/${imageId}`, { method: 'DELETE' });
  if (!response.ok && response.status !== 404) {
    throw await errorFromResponse(response);
  }
}

/**
 * Downloads the raw samples of an image with transfer "raw". Progress counts decoded bytes against the size implied
 * by the image info, because Content-Length is the compressed size.
 */
export async function fetchRawImage(info: ImageInfo, onProgress?: ProgressCallback, signal?: AbortSignal): Promise<RawImage> {
  const response = await fetch(`${API_PREFIX}/images/${info.imageId}/raw`, { signal });
  if (!response.ok) {
    throw await errorFromResponse(response);
  }
  const format = rawFormatFromHeaders(response.headers);
  const total = format.width * format.height * (format.bitDepth / 8);

  if (!response.body) {
    return decodeRawSamples(await response.arrayBuffer(), format);
  }

  // Read into one preallocated buffer; a longer body than expected is an error, a shorter one fails in decodeRawSamples
  const buffer = new ArrayBuffer(total);
  const bytes = new Uint8Array(buffer);
  let loaded = 0;
  const reader = response.body.getReader();
  for (;;) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    if (loaded + value.length > total) {
      await reader.cancel();
      throw new Error(`Raw data is longer than the expected ${total} bytes`);
    }
    bytes.set(value, loaded);
    loaded += value.length;
    onProgress?.(loaded, total);
  }
  return decodeRawSamples(loaded === total ? buffer : buffer.slice(0, loaded), format);
}

export interface DisplayOptions {
  min: number;
  max: number;
  maxSize?: number;
}

export function displayUrl(imageId: string, { min, max, maxSize }: DisplayOptions): string {
  const query = new URLSearchParams({ min: String(min), max: String(max) });
  if (maxSize !== undefined) {
    query.set('maxSize', String(maxSize));
  }
  return `${API_PREFIX}/images/${imageId}/display.png?${query}`;
}

export async function fetchDisplayBlob(imageId: string, options: DisplayOptions, signal?: AbortSignal): Promise<Blob> {
  const response = await fetch(displayUrl(imageId, options), { signal });
  if (!response.ok) {
    throw await errorFromResponse(response);
  }
  return response.blob();
}

export function getPixel(imageId: string, x: number, y: number, signal?: AbortSignal): Promise<PixelResponse> {
  return getJson(`${API_PREFIX}/images/${imageId}/pixel?x=${x}&y=${y}`, signal);
}

async function sendJson<T>(method: 'POST' | 'DELETE', url: string, body?: unknown, signal?: AbortSignal): Promise<T> {
  const response = await fetch(url, {
    method,
    signal,
    headers: body === undefined ? { accept: 'application/json' } : { accept: 'application/json', 'content-type': 'application/json' },
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  if (!response.ok) {
    throw await errorFromResponse(response);
  }
  return (response.status === 204 ? undefined : await response.json()) as T;
}

export function getRoiStats(imageId: string, rois: RoiStatsRequest['rois'], signal?: AbortSignal): Promise<RoiStatsResponse> {
  return sendJson('POST', `${API_PREFIX}/images/${imageId}/roi-stats`, { rois }, signal);
}

export function startAnalysis(request: AnalysisRequest): Promise<AnalysisInfo> {
  return sendJson('POST', `${API_PREFIX}/analyses`, request);
}

export function getAnalysisResults(analysisId: string, signal?: AbortSignal): Promise<AnalysisResults> {
  return getJson(`${API_PREFIX}/analyses/${analysisId}/results`, signal);
}

export function cancelAnalysis(analysisId: string): Promise<void> {
  return sendJson('DELETE', `${API_PREFIX}/analyses/${analysisId}`);
}

export function analysisEventsUrl(analysisId: string): string {
  return `${API_PREFIX}/analyses/${analysisId}/events`;
}

export interface DownloadedFile {
  blob: Blob;
  fileName: string;
}

async function postForFile(url: string, body: unknown, fallbackName: string): Promise<DownloadedFile> {
  const response = await fetch(url, { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify(body) });
  if (!response.ok) {
    throw await errorFromResponse(response);
  }
  return { blob: await response.blob(), fileName: fileNameFromDisposition(response.headers.get('content-disposition'), fallbackName) };
}

export function exportResults(format: ExportFormat, documents: ResultsDocument[]): Promise<DownloadedFile> {
  return postForFile(`${API_PREFIX}/exports/results`, { format, documents }, `results.${format}`);
}

export function exportRoiImages(request: RoiImagesExportRequest): Promise<DownloadedFile> {
  return postForFile(`${API_PREFIX}/exports/roi-images`, request, 'rois.zip');
}

export async function findImagesBySha256(sha256: string, signal?: AbortSignal): Promise<ImageInfo[]> {
  return (await getJson<ImageListResponse>(`${API_PREFIX}/images?sha256=${sha256}`, signal)).images;
}

/** The uploaded file of an image */
export async function downloadOriginal(imageId: string): Promise<Uint8Array> {
  const response = await fetch(`${API_PREFIX}/images/${imageId}/original`);
  if (!response.ok) {
    throw await errorFromResponse(response);
  }
  return new Uint8Array(await response.arrayBuffer());
}

export async function getCoreVersion(): Promise<string> {
  return (await getJson<HealthResponse>(`${API_PREFIX}/health`)).coreVersion;
}
