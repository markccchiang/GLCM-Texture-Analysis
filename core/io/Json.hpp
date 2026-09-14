#ifndef GLCM_JSON_HPP_
#define GLCM_JSON_HPP_

#include <string>
#include <vector>

#include "pipeline/AnalysisRunner.hpp"
#include "pipeline/AnalysisSettings.hpp"
#include "roi/Roi.hpp"

namespace glcm {

// Image an ROI set was drawn on, used to warn when it is imported onto a different image
struct ImageReference {
    std::string name;
    int width = 0;
    int height = 0;
    int bit_depth = 0;
    std::string sha256;
};

// {"format": "glcm-roi-set", "version": 1, "image": {...}, "rois": [...]} (doc/ui-design-plan.md, section 8.4)
struct RoiSetDocument {
    ImageReference image;
    std::vector<Roi> rois;
};

// Metadata written into exported results
struct ExportContext {
    std::string image_name;
    std::string image_sha256;
    std::string timestamp; // ISO 8601, supplied by the caller
};

std::string RoiSetToJson(const RoiSetDocument& document);

// Throws std::invalid_argument ("Invalid ROI set: <path> <problem>") for malformed JSON, another format, an unsupported
// version or invalid ROIs
RoiSetDocument RoiSetFromJson(const std::string& text);

// Settings object of an analysis request, e.g. {"features": ["Contrast"], "grayLevels": 32, "distances": [1], ...}
std::string SettingsToJson(const AnalysisSettings& settings);

// Parses a settings object; fields other than "features" are optional and keep their AnalysisSettings defaults.
// Throws std::invalid_argument ("Invalid analysis settings: <path> <problem>"). The result is not validated; call
// ValidateSettings before running an analysis.
AnalysisSettings SettingsFromJson(const std::string& text);

// {"format": "glcm-results", "version": 1, "coreVersion", "timestamp", "image", "settings", "results": [...]}.
// Feature values are objects {"0", "45", "90", "135", "mean", "range"}; NaN values are written as null.
std::string ResultsToJson(const std::vector<MeasurementResult>& results, const AnalysisSettings& settings, const ExportContext& context);

} // namespace glcm

#endif // GLCM_JSON_HPP_
