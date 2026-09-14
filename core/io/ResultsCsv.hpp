#ifndef GLCM_RESULTS_CSV_HPP_
#define GLCM_RESULTS_CSV_HPP_

#include <string>
#include <vector>

#include "io/Json.hpp"
#include "pipeline/AnalysisRunner.hpp"
#include "pipeline/AnalysisSettings.hpp"

namespace glcm {

// Complete CSV file of analysis results:
// - "# key=value" lines with the format, core version, image and settings
// - a header row: timestamp, image, imageSha256, roiName, roiId, status, pixelCount, grayLevels, quantization,
//   distance, direction, one column per selected feature (catalog order; non-standard ones end with
//   " [non-standard]"), Score (when enabled), warnings
// - for each successful measurement, rows chosen by settings.aggregation: one per selected direction ("0", "45", "90",
//   "135") and "mean"; only "mean"; or "mean" and "range"
// - for each skipped or failed measurement, one row with an empty direction and the reason in "warnings"
// Numbers use the shortest text that reads back to the same double; NaN cells are empty. Fields are quoted per RFC 4180.
std::string ResultsToCsv(const std::vector<MeasurementResult>& results, const AnalysisSettings& settings, const ExportContext& context);

} // namespace glcm

#endif // GLCM_RESULTS_CSV_HPP_
