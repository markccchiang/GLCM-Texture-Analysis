#include "io/Identifiers.hpp"

#include <stdexcept>
#include <utility>

namespace glcm {

namespace {

template <typename Enum, size_t Size>
std::string IdOf(Enum value, const std::array<std::pair<Enum, const char*>, Size>& table) {
    for (const auto& [entry, id] : table) {
        if (entry == value) {
            return id;
        }
    }
    throw std::invalid_argument("Unknown enumeration value");
}

template <typename Enum, size_t Size>
std::optional<Enum> ValueOf(const std::string& id, const std::array<std::pair<Enum, const char*>, Size>& table) {
    for (const auto& [entry, entry_id] : table) {
        if (id == entry_id) {
            return entry;
        }
    }
    return std::nullopt;
}

const std::array<std::pair<Direction, const char*>, 4> DIRECTION_ANGLES{{
    {Direction::H, "0"},
    {Direction::RD, "45"},
    {Direction::V, "90"},
    {Direction::LD, "135"},
}};

const std::array<std::pair<QuantizationMethod, const char*>, 4> QUANTIZATION_METHODS{{
    {QuantizationMethod::FixedRange, "fixedRange"},
    {QuantizationMethod::RoiMinMax, "roiMinMax"},
    {QuantizationMethod::FixedBinWidth, "fixedBinWidth"},
    {QuantizationMethod::None, "none"},
}};

const std::array<std::pair<Aggregation, const char*>, 3> AGGREGATIONS{{
    {Aggregation::PerDirectionAndMean, "perDirectionAndMean"},
    {Aggregation::MeanOnly, "meanOnly"},
    {Aggregation::MeanAndRange, "meanAndRange"},
}};

const std::array<std::pair<LogBase, const char*>, 2> LOG_BASES{{
    {LogBase::Natural, "natural"},
    {LogBase::Two, "log2"},
}};

const std::array<std::pair<ScoreProfile, const char*>, 2> SCORE_PROFILES{{
    {ScoreProfile::Calibration, "calibration"},
    {ScoreProfile::CurrentSettings, "currentSettings"},
}};

const std::array<std::pair<MeasurementStatus, const char*>, 3> MEASUREMENT_STATUSES{{
    {MeasurementStatus::Ok, "ok"},
    {MeasurementStatus::Skipped, "skipped"},
    {MeasurementStatus::Failed, "failed"},
}};

} // namespace

int DirectionAngle(Direction direction) {
    if (direction == Direction::Avg) {
        throw std::invalid_argument("Avg has no angle");
    }
    return std::stoi(IdOf(direction, DIRECTION_ANGLES));
}

std::optional<Direction> DirectionFromAngle(int angle) {
    return ValueOf(std::to_string(angle), DIRECTION_ANGLES);
}

std::string QuantizationMethodId(QuantizationMethod method) {
    return IdOf(method, QUANTIZATION_METHODS);
}

std::optional<QuantizationMethod> QuantizationMethodFromId(const std::string& id) {
    return ValueOf(id, QUANTIZATION_METHODS);
}

std::string AggregationId(Aggregation aggregation) {
    return IdOf(aggregation, AGGREGATIONS);
}

std::optional<Aggregation> AggregationFromId(const std::string& id) {
    return ValueOf(id, AGGREGATIONS);
}

std::string LogBaseId(LogBase log_base) {
    return IdOf(log_base, LOG_BASES);
}

std::optional<LogBase> LogBaseFromId(const std::string& id) {
    return ValueOf(id, LOG_BASES);
}

std::string ScoreProfileId(ScoreProfile profile) {
    return IdOf(profile, SCORE_PROFILES);
}

std::optional<ScoreProfile> ScoreProfileFromId(const std::string& id) {
    return ValueOf(id, SCORE_PROFILES);
}

std::string MeasurementStatusId(MeasurementStatus status) {
    return IdOf(status, MEASUREMENT_STATUSES);
}

} // namespace glcm
