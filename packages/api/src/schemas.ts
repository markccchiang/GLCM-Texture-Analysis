// Request and response schemas of the /api/v1 HTTP API (doc/ui-design-plan.md, section 8.5). The server validates
// requests and serializes responses with them, the OpenAPI document is generated from them, and the web app uses the
// derived types.

import { Type, type Static } from 'typebox';

export const API_PREFIX = '/api/v1';

export const ErrorResponse = Type.Object(
  {
    error: Type.String({ description: 'Machine-readable error code, e.g. "NotFound" or "PayloadTooLarge"' }),
    message: Type.String({ description: 'Human-readable description' }),
  },
  { description: 'Error returned by every endpoint for 4xx and 5xx responses' },
);
export type ErrorResponse = Static<typeof ErrorResponse>;

// ---------------------------------------------------------------------------------------------------------------------
// Health and catalog
// ---------------------------------------------------------------------------------------------------------------------

export const HealthResponse = Type.Object({
  status: Type.Literal('ok'),
  coreVersion: Type.String(),
});
export type HealthResponse = Static<typeof HealthResponse>;

export const FeatureGroup = Type.Union([Type.Literal('regionStatistics'), Type.Literal('haralick'), Type.Literal('other')]);

export const FeatureInfo = Type.Object({
  id: Type.String({ description: 'Stable identifier used in analysis settings, e.g. "CorrelationII"' }),
  name: Type.String(),
  group: FeatureGroup,
  nonStandard: Type.Boolean({ description: 'Differs from the literature definition; see nonStandardReason' }),
  nonStandardReason: Type.String(),
  docAnchor: Type.String({ description: 'Page and anchor in the Sphinx documentation' }),
  cost: Type.Union([Type.Literal('normal'), Type.Literal('slow')]),
});
export type FeatureInfo = Static<typeof FeatureInfo>;

export const FeaturePreset = Type.Object({
  id: Type.String(),
  name: Type.String(),
  features: Type.Array(Type.String()),
  enablesScore: Type.Boolean(),
});
export type FeaturePreset = Static<typeof FeaturePreset>;

export const ScoreCoefficients = Type.Object({
  age: Type.Number(),
  mean: Type.Number(),
  entropy: Type.Number(),
  contrast: Type.Number(),
});
export type ScoreCoefficients = Static<typeof ScoreCoefficients>;

export const AnalysisLimits = Type.Object({
  minGrayLevels: Type.Integer(),
  maxGrayLevels: Type.Integer(),
  defaultGrayLevels: Type.Integer(),
  maxDistance: Type.Integer(),
  directions: Type.Array(Type.Integer()),
  quantizationMethods: Type.Array(Type.String()),
  aggregations: Type.Array(Type.String()),
  logBases: Type.Array(Type.String()),
  scoreProfiles: Type.Array(Type.String()),
  defaultScoreCoefficients: ScoreCoefficients,
});
export type AnalysisLimits = Static<typeof AnalysisLimits>;

export const UploadLimits = Type.Object({
  maxUploadBytes: Type.Integer(),
  maxImagePixels: Type.Integer(),
  rawTransferMaxPixels: Type.Integer({ description: 'Images up to this many pixels are sent to the browser as raw data' }),
  displayMaxSize: Type.Integer({ description: 'Largest long side of display.png' }),
});
export type UploadLimits = Static<typeof UploadLimits>;

export const CatalogResponse = Type.Object({
  features: Type.Array(FeatureInfo),
  presets: Type.Array(FeaturePreset),
  limits: AnalysisLimits,
  uploads: UploadLimits,
});
export type CatalogResponse = Static<typeof CatalogResponse>;

// ---------------------------------------------------------------------------------------------------------------------
// Images
// ---------------------------------------------------------------------------------------------------------------------

export const IMAGE_ID_PATTERN = '^img_[0-9a-f]{32}$';

export const ImageIdParams = Type.Object({
  id: Type.String({ pattern: IMAGE_ID_PATTERN, description: 'Image id returned by POST /images' }),
});
export type ImageIdParams = Static<typeof ImageIdParams>;

export const ImageInfo = Type.Object({
  imageId: Type.String({ pattern: IMAGE_ID_PATTERN }),
  name: Type.String({ description: 'File name of the upload' }),
  sizeBytes: Type.Integer({ description: 'Size of the uploaded file' }),
  width: Type.Integer(),
  height: Type.Integer(),
  bitDepth: Type.Union([Type.Literal(8), Type.Literal(16)]),
  sourceChannels: Type.Integer({ description: 'Channels before grayscale conversion (1 or 3)' }),
  sha256: Type.String({ description: 'SHA-256 of the uploaded file (hex)' }),
  transfer: Type.Union([Type.Literal('raw'), Type.Literal('server')], {
    description: '"raw": GET /raw is available and the browser renders the image; "server": use display.png and /pixel',
  }),
  windowMin: Type.Integer({ description: 'Default display window: 0.5th percentile' }),
  windowMax: Type.Integer({ description: 'Default display window: 99.5th percentile' }),
  histogram: Type.Array(Type.Integer(), { minItems: 256, maxItems: 256, description: '256 equal bins over 0-255 or 0-65535' }),
  warnings: Type.Array(Type.String()),
  createdAt: Type.String({ format: 'date-time' }),
});
export type ImageInfo = Static<typeof ImageInfo>;

export const DisplayQuery = Type.Object({
  min: Type.Optional(Type.Integer({ minimum: 0, maximum: 65535, description: 'Window minimum; default windowMin' })),
  max: Type.Optional(Type.Integer({ minimum: 0, maximum: 65535, description: 'Window maximum; default windowMax' })),
  maxSize: Type.Optional(Type.Integer({ minimum: 1, maximum: 16384, description: 'Largest long side; capped by the server limit' })),
});
export type DisplayQuery = Static<typeof DisplayQuery>;

export const PixelQuery = Type.Object({
  x: Type.Integer({ minimum: 0 }),
  y: Type.Integer({ minimum: 0 }),
});
export type PixelQuery = Static<typeof PixelQuery>;

export const PixelResponse = Type.Object({
  x: Type.Integer(),
  y: Type.Integer(),
  value: Type.Integer({ description: 'Grayscale value of the stored image' }),
});
export type PixelResponse = Static<typeof PixelResponse>;

// Headers of GET /images/{id}/raw
export const RAW_HEADERS = {
  width: 'x-image-width',
  height: 'x-image-height',
  bitDepth: 'x-image-bit-depth',
  byteOrder: 'x-image-byte-order',
} as const;
