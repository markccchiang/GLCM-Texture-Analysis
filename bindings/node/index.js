// Loads the glcm_native addon built by `npm run build -w @glcm/native`
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const native = require('./build/Release/glcm_native.node');

export const {
  coreVersion,
  catalog,
  decodeImageFile,
  renderDisplay,
  roiStats,
  validateAnalysis,
  runAnalysis,
  formatResults,
  exportRoiImages,
  windowLevel,
  featureMapGrid,
  computeFeatureMap,
} = native;
export default native;
