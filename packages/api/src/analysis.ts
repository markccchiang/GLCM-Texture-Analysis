// ROI, ROI statistics and analysis schemas (doc/ui-design-plan.md, sections 6.2, 6.3, 8.4 and 8.5). Field names follow
// the JSON written and read by glcm_core (core/io/Json.cpp).

import { Type, type Static } from 'typebox';
import { IMAGE_ID_PATTERN, PixelSpacing } from './schemas.js';

// Limits from doc/ui-design-plan.md, section 8.2
export const MAX_ROIS_PER_REQUEST = 1000;
export const MAX_POLYGON_VERTICES = 10000;

const Coordinate = Type.Number({ description: 'Image pixel coordinate; pixel (c, r) covers [c, c + 1) × [r, r + 1)' });

// ---------------------------------------------------------------------------------------------------------------------
// ROIs
// ---------------------------------------------------------------------------------------------------------------------

export const RectangleShape = Type.Object({
  type: Type.Literal('rectangle'),
  x: Coordinate,
  y: Coordinate,
  width: Type.Number(),
  height: Type.Number(),
});
export type RectangleShape = Static<typeof RectangleShape>;

export const EllipseShape = Type.Object({
  type: Type.Literal('ellipse'),
  cx: Coordinate,
  cy: Coordinate,
  rx: Type.Number({ description: 'Semi-axis along the ellipse x axis' }),
  ry: Type.Number({ description: 'Semi-axis along the ellipse y axis' }),
  angle: Type.Optional(Type.Number({ description: 'Rotation in degrees, clockwise on screen' })),
});
export type EllipseShape = Static<typeof EllipseShape>;

export const PolygonShape = Type.Object({
  type: Type.Literal('polygon'),
  points: Type.Array(Type.Tuple([Coordinate, Coordinate]), { maxItems: MAX_POLYGON_VERTICES }),
  freehand: Type.Optional(Type.Boolean()),
});
export type PolygonShape = Static<typeof PolygonShape>;

export const RoiShape = Type.Union([RectangleShape, EllipseShape, PolygonShape]);
export type RoiShape = Static<typeof RoiShape>;

export const Roi = Type.Object({
  id: Type.String({ maxLength: 100 }),
  name: Type.String({ maxLength: 200 }),
  color: Type.Optional(Type.String({ pattern: '^#[0-9A-Fa-f]{6}$' })),
  shape: RoiShape,
});
export type Roi = Static<typeof Roi>;

// ---------------------------------------------------------------------------------------------------------------------
// ROI statistics
// ---------------------------------------------------------------------------------------------------------------------

export const RoiStatsRequest = Type.Object({
  rois: Type.Array(Type.Object({ id: Type.String({ maxLength: 100 }), shape: RoiShape }), { maxItems: MAX_ROIS_PER_REQUEST }),
});
export type RoiStatsRequest = Static<typeof RoiStatsRequest>;

const NullableNumber = Type.Union([Type.Number(), Type.Null()]);

export const RoiStatistics = Type.Object({
  roiId: Type.String(),
  pixelCount: Type.Integer({ description: 'Pixels whose centre lies inside the shape, clipped to the image' }),
  boundingBox: Type.Union([
    Type.Object({ x: Type.Integer(), y: Type.Integer(), width: Type.Integer(), height: Type.Integer() }),
    Type.Null(),
  ]),
  min: NullableNumber,
  max: NullableNumber,
  mean: NullableNumber,
  std: NullableNumber,
  error: Type.Union([Type.String(), Type.Null()], { description: 'Invalid geometry' }),
});
export type RoiStatistics = Static<typeof RoiStatistics>;

export const RoiStatsResponse = Type.Object({ stats: Type.Array(RoiStatistics) });
export type RoiStatsResponse = Static<typeof RoiStatsResponse>;

// ---------------------------------------------------------------------------------------------------------------------
// Analysis settings
// ---------------------------------------------------------------------------------------------------------------------

export const QuantizationMethod = Type.Union([
  Type.Literal('fixedRange'),
  Type.Literal('roiMinMax'),
  Type.Literal('fixedBinWidth'),
  Type.Literal('none'),
]);
export type QuantizationMethod = Static<typeof QuantizationMethod>;

export const Direction = Type.Union([Type.Literal(0), Type.Literal(45), Type.Literal(90), Type.Literal(135)]);
export type Direction = Static<typeof Direction>;

export const Aggregation = Type.Union([Type.Literal('perDirectionAndMean'), Type.Literal('meanOnly'), Type.Literal('meanAndRange')]);
export type Aggregation = Static<typeof Aggregation>;

export const ScoreProfile = Type.Union([Type.Literal('calibration'), Type.Literal('currentSettings')]);
export type ScoreProfile = Static<typeof ScoreProfile>;

export const AnalysisSettings = Type.Object({
  features: Type.Array(Type.String(), { minItems: 1, maxItems: 64, description: 'Feature ids from GET /catalog' }),
  grayLevels: Type.Integer({ minimum: 2, maximum: 256 }),
  quantization: Type.Object({
    method: QuantizationMethod,
    min: Type.Integer({ minimum: 0, maximum: 65535, description: 'fixedRange: lowest intensity' }),
    max: Type.Integer({ minimum: 0, maximum: 65535, description: 'fixedRange: highest intensity' }),
    binWidth: Type.Number({ minimum: 0, description: 'fixedBinWidth: intensities per gray level' }),
  }),
  distances: Type.Array(Type.Integer({ minimum: 1, maximum: 64 }), { minItems: 1, maxItems: 64 }),
  directions: Type.Array(Direction, { minItems: 1, maxItems: 4 }),
  aggregation: Aggregation,
  logBase: Type.Union([Type.Literal('natural'), Type.Literal('log2')]),
  score: Type.Object({
    enabled: Type.Boolean(),
    age: Type.Number(),
    coefficients: Type.Tuple([Type.Number(), Type.Number(), Type.Number(), Type.Number()], {
      description: 'Age, mean, entropy and contrast coefficients',
    }),
    profile: ScoreProfile,
    intensityMin: Type.Integer({ minimum: 0, maximum: 65535, description: 'Calibration profile on 16-bit images: mapped to 0' }),
    intensityMax: Type.Integer({ minimum: 0, maximum: 65535, description: 'Calibration profile on 16-bit images: mapped to 255' }),
  }),
});
export type AnalysisSettings = Static<typeof AnalysisSettings>;

// ---------------------------------------------------------------------------------------------------------------------
// Analyses
// ---------------------------------------------------------------------------------------------------------------------

export const ANALYSIS_ID_PATTERN = '^ana_[0-9a-f]{32}$';

export const AnalysisIdParams = Type.Object({ id: Type.String({ pattern: ANALYSIS_ID_PATTERN }) });
export type AnalysisIdParams = Static<typeof AnalysisIdParams>;

export const AnalysisRequest = Type.Object({
  imageId: Type.String({ pattern: IMAGE_ID_PATTERN }),
  rois: Type.Array(Roi, { minItems: 1, maxItems: MAX_ROIS_PER_REQUEST }),
  settings: AnalysisSettings,
  pixelSpacing: Type.Optional(
    Type.Union([PixelSpacing, Type.Null()], {
      description: "Pixel spacing for the ROI areas in the results, e.g. entered by the user; null for none. Omitted: the image's own pixelSpacing",
    }),
  ),
});
export type AnalysisRequest = Static<typeof AnalysisRequest>;

export const FeatureValues = Type.Object(
  {
    '0': NullableNumber,
    '45': NullableNumber,
    '90': NullableNumber,
    '135': NullableNumber,
    mean: NullableNumber,
    range: NullableNumber,
  },
  { description: 'Per direction, plus mean and range over the selected directions; null for unselected directions' },
);
export type FeatureValues = Static<typeof FeatureValues>;

export const MeasurementStatus = Type.Union([Type.Literal('ok'), Type.Literal('skipped'), Type.Literal('failed')]);
export type MeasurementStatus = Static<typeof MeasurementStatus>;

export const MeasurementResult = Type.Object({
  roiId: Type.String(),
  roiName: Type.String(),
  distance: Type.Integer(),
  status: MeasurementStatus,
  error: Type.String({ description: 'Reason for skipped or failed results' }),
  pixelCount: Type.Integer(),
  pairCounts: Type.Object({ '0': Type.Integer(), '45': Type.Integer(), '90': Type.Integer(), '135': Type.Integer() }),
  quantization: Type.Object({ lower: Type.Integer(), upper: Type.Integer() }),
  values: Type.Record(Type.String(), FeatureValues),
  score: Type.Union([FeatureValues, Type.Null()]),
  warnings: Type.Array(Type.String()),
});
export type MeasurementResult = Static<typeof MeasurementResult>;

export const AnalysisStatus = Type.Union([
  Type.Literal('queued'),
  Type.Literal('running'),
  Type.Literal('completed'),
  Type.Literal('cancelled'),
  Type.Literal('failed'),
]);
export type AnalysisStatus = Static<typeof AnalysisStatus>;

export const AnalysisInfo = Type.Object({
  analysisId: Type.String({ pattern: ANALYSIS_ID_PATTERN }),
  imageId: Type.String({ pattern: IMAGE_ID_PATTERN }),
  imageName: Type.String(),
  imageSha256: Type.String(),
  status: AnalysisStatus,
  total: Type.Integer({ description: 'Jobs: ROIs × distances' }),
  completed: Type.Integer(),
  error: Type.Union([Type.String(), Type.Null()], { description: 'Why the whole analysis failed' }),
  createdAt: Type.String({ format: 'date-time' }),
  finishedAt: Type.Union([Type.String({ format: 'date-time' }), Type.Null()]),
  coreVersion: Type.String(),
  settings: AnalysisSettings,
  pixelSpacing: Type.Union([PixelSpacing, Type.Null()], { description: 'Spacing of the analysis: from the request, else from the image' }),
});
export type AnalysisInfo = Static<typeof AnalysisInfo>;

export const AnalysisResults = Type.Object({
  format: Type.Literal('glcm-results'),
  version: Type.Literal(1),
  analysisId: Type.String({ pattern: ANALYSIS_ID_PATTERN }),
  status: AnalysisStatus,
  coreVersion: Type.String(),
  timestamp: Type.String({ format: 'date-time' }),
  image: Type.Object({
    id: Type.String(),
    name: Type.String(),
    sha256: Type.String(),
    pixelSpacing: Type.Optional(PixelSpacing),
  }),
  settings: AnalysisSettings,
  results: Type.Array(MeasurementResult, { description: 'Ordered by ROI, then distance; only finished jobs' }),
});
export type AnalysisResults = Static<typeof AnalysisResults>;

// Server-Sent Events of GET /analyses/{id}/events
export interface AnalysisProgressEvent {
  completed: number;
  total: number;
}
export interface AnalysisResultEvent {
  /** Position in the final result order (ROI index × distance count + distance index) */
  index: number;
  result: MeasurementResult;
}
export interface AnalysisFinishedEvent {
  status: AnalysisStatus;
  completed: number;
  total: number;
  error: string | null;
}
export type AnalysisEvent =
  | { event: 'progress'; data: AnalysisProgressEvent }
  | { event: 'result'; data: AnalysisResultEvent }
  | { event: 'finished'; data: AnalysisFinishedEvent };
