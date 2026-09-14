// Types of the glcm_native addon (bindings/node/src/addon.cpp).
// Rejected promises and thrown errors carry `code`: INVALID_ARGUMENT, UNSUPPORTED_IMAGE, DECODE_FAILED or
// INTERNAL_ERROR. Invalid argument types throw a TypeError synchronously.

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

/** Decodes an image file (PNG, JPEG, BMP, 8/16-bit TIFF, ...); color is converted to grayscale. */
export function decodeImageFile(path: string): Promise<DecodedImage>;

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
  windowLevel: typeof windowLevel;
};
export default native;
