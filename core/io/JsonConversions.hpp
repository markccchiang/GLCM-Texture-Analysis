#ifndef GLCM_JSON_CONVERSIONS_HPP_
#define GLCM_JSON_CONVERSIONS_HPP_

// Internal to glcm_core: nlohmann::json conversions shared by the io sources. Public headers do not include this file,
// so users of glcm_core do not need nlohmann/json.

#include <nlohmann/json.hpp>
#include <string>

#include "analysis/TextureAnalysis.hpp"
#include "pipeline/AnalysisSettings.hpp"
#include "roi/Roi.hpp"

namespace glcm::json_detail {

using Json = nlohmann::ordered_json;

Json RoiToJson(const Roi& roi);

// path is used in error messages, e.g. "rois[2]"
Roi RoiFromJson(const Json& value, const std::string& path);

Json SettingsToJsonValue(const AnalysisSettings& settings);

AnalysisSettings SettingsFromJsonValue(const Json& value, const std::string& path);

// {"0": H, "45": RD, "90": V, "135": LD, "mean": Avg(), "range": Range()}; NaN becomes null
Json FeaturesToJson(const Features& features);

// Reads the directions of FeaturesToJson; null or missing directions become NaN, "mean" and "range" are ignored
Features FeaturesFromJson(const Json& value, const std::string& path);

} // namespace glcm::json_detail

#endif // GLCM_JSON_CONVERSIONS_HPP_
