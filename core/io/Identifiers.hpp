#ifndef GLCM_IDENTIFIERS_HPP_
#define GLCM_IDENTIFIERS_HPP_

#include <array>
#include <optional>
#include <string>

#include "analysis/TextureAnalysis.hpp"
#include "imaging/Quantizer.hpp"
#include "pipeline/AnalysisRunner.hpp"
#include "pipeline/AnalysisSettings.hpp"

namespace glcm {

// Text identifiers used in JSON, CSV and the web API

// Directions in file order: 0, 45, 90 and 135 degrees
inline constexpr std::array<Direction, 4> DIRECTIONS_BY_ANGLE{Direction::H, Direction::RD, Direction::V, Direction::LD};

// Angle in degrees: H = 0, RD = 45, V = 90, LD = 135. Throws std::invalid_argument for Avg.
int DirectionAngle(Direction direction);
std::optional<Direction> DirectionFromAngle(int angle);

std::string QuantizationMethodId(QuantizationMethod method); // "fixedRange", "roiMinMax", "fixedBinWidth", "none"
std::optional<QuantizationMethod> QuantizationMethodFromId(const std::string& id);

std::string AggregationId(Aggregation aggregation); // "perDirectionAndMean", "meanOnly", "meanAndRange"
std::optional<Aggregation> AggregationFromId(const std::string& id);

std::string LogBaseId(LogBase log_base); // "natural", "log2"
std::optional<LogBase> LogBaseFromId(const std::string& id);

std::string ScoreProfileId(ScoreProfile profile); // "calibration", "currentSettings"
std::optional<ScoreProfile> ScoreProfileFromId(const std::string& id);

std::string MeasurementStatusId(MeasurementStatus status); // "ok", "skipped", "failed"

} // namespace glcm

#endif // GLCM_IDENTIFIERS_HPP_
