// Types of the glcm_native addon (bindings/node/src/addon.cpp).
// Rejected promises and thrown errors carry `code`: INVALID_ARGUMENT, UNSUPPORTED_IMAGE, IMAGE_TOO_LARGE,
// DECODE_FAILED or INTERNAL_ERROR. Invalid argument types throw a TypeError synchronously.

export type FeatureGroupId = 'regionStatistics' | 'haralick' | 'other';

export interface NativeFeatureInfo {
  id: string;
  name: string;
  group: FeatureGroupId;
  nonStandard: boolean;
  nonStandardReason: string;
  docAnchor: string;
  cost: 'normal' | 'slow';
}

export interface NativeFeaturePreset {
  id: string;
  name: string;
  features: string[];
  enablesScore: boolean;
}

export interface NativeCatalog {
  features: NativeFeatureInfo[];
  presets: NativeFeaturePreset[];
  limits: {
    minGrayLevels: number;
    maxGrayLevels: number;
    defaultGrayLevels: number;
    maxDistance: number;
    directions: number[];
    quantizationMethods: string[];
    aggregations: string[];
    logBases: string[];
    scoreProfiles: string[];
    defaultScoreCoefficients: { age: number; mean: number; entropy: number; contrast: number };
  };
}

export interface DecodedImage {
  width: number;
  height: number;
  bitDepth: 8 | 16;
  /** Channels of the file before grayscale conversion (1 or 3) */
  sourceChannels: number;
  warnings: string[];
  /** 0.5th and 99.5th percentiles (nearest rank) */
  windowMin: number;
  windowMax: number;
  /** 256 equal bins over the full range of the bit depth */
  histogram: number[];
  /** Row-major grayscale samples; 16-bit samples are little-endian */
  pixels: Buffer;
}

export interface NativeRoiStatistics {
  /** Pixels whose centre lies inside the shape, after clipping to the image */
  pixelCount: number;
  boundingBox: { x: number; y: number; width: number; height: number } | null;
  /** Original intensities; null for an empty mask */
  min: number | null;
  max: number | null;
  mean: number | null;
  /** Sample standard deviation */
  std: number | null;
  /** Invalid geometry (e.g. non-finite coordinates); the other fields then describe an empty mask */
  error: string | null;
}

export function coreVersion(): string;

export function catalog(): NativeCatalog;

export interface DecodeOptions {
  /**
   * Largest width × height. The size is read from the file header before decoding, so larger images reject with
   * IMAGE_TOO_LARGE without allocating memory for their pixels; files that are not PNG, JPEG, BMP or TIFF then reject
   * with DECODE_FAILED. 0 or absent: no limit.
   */
  maxPixels?: number;
}

/** Decodes an image file (PNG, JPEG, BMP, 8/16-bit TIFF, ...); color is converted to grayscale. */
export function decodeImageFile(path: string, options?: DecodeOptions): Promise<DecodedImage>;

/** 8-bit PNG of the pixels with the window/level mapping, downscaled so the long side is at most maxSize (0 = no limit). */
export function renderDisplay(
  pixels: Uint8Array,
  width: number,
  height: number,
  bitDepth: 8 | 16,
  windowMin: number,
  windowMax: number,
  maxSize: number,
): Promise<Buffer>;

/**
 * Pixel count, bounding box and intensity statistics of each ROI.
 * @param roisJson JSON array of ROI objects in the ROI set format (doc/ui-design-plan.md, section 8.4)
 */
export function roiStats(pixels: Uint8Array, width: number, height: number, bitDepth: 8 | 16, roisJson: string): Promise<NativeRoiStatistics[]>;

/** Parses and validates an analysis request; throws an Error with code INVALID_ARGUMENT describing the first problem. */
export function validateAnalysis(roisJson: string, settingsJson: string): void;

/**
 * Measures every ROI at every distance (glcm::RunAnalysis).
 * @returns the "glcm-results" JSON document (without image name, SHA-256 or timestamp)
 */
export function runAnalysis(
  pixels: Uint8Array,
  width: number,
  height: number,
  bitDepth: 8 | 16,
  roisJson: string,
  settingsJson: string,
): Promise<string>;

/**
 * Reads a "glcm-results" document and writes it again with glcm_core: "csv" (glcm::ResultsToCsv) or canonical "json"
 * (glcm::ResultsToJson). Throws an Error with code INVALID_ARGUMENT for invalid documents.
 */
export function formatResults(resultsJson: string, format: 'csv' | 'json'): string;

export interface ExportedFile {
  name: string;
  data: Buffer;
}

/**
 * ROI crops, masks, optional quantized images and manifest.json (glcm::ExportRoiImages).
 * @param settingsJson analysis settings, or "" when includeQuantized is false
 */
export function exportRoiImages(
  pixels: Uint8Array,
  width: number,
  height: number,
  bitDepth: 8 | 16,
  roisJson: string,
  settingsJson: string,
  transparentOutside: boolean,
  includeQuantized: boolean,
): Promise<ExportedFile[]>;

/** glcm::WindowLevel: the 8-bit display value of one intensity. */
export function windowLevel(value: number, windowMin: number, windowMax: number): number;

declare const native: {
  coreVersion: typeof coreVersion;
  catalog: typeof catalog;
  decodeImageFile: typeof decodeImageFile;
  renderDisplay: typeof renderDisplay;
  roiStats: typeof roiStats;
  validateAnalysis: typeof validateAnalysis;
  runAnalysis: typeof runAnalysis;
  formatResults: typeof formatResults;
  exportRoiImages: typeof exportRoiImages;
  windowLevel: typeof windowLevel;
};
export default native;
