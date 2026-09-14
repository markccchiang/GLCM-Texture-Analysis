#include "io/Json.hpp"

#include <climits>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#include "io/Identifiers.hpp"
#include "io/JsonConversions.hpp"
#include "pipeline/FeatureCatalog.hpp"
#include "pipeline/Version.hpp"

namespace glcm {

using json_detail::Json;

namespace {

[[noreturn]] void Fail(const std::string& path, const std::string& problem) {
    throw std::invalid_argument((path.empty() ? std::string("document") : path) + " " + problem);
}

std::string Child(const std::string& path, const std::string& key) {
    return path.empty() ? key : path + "." + key;
}

std::string Index(const std::string& path, size_t index) {
    return path + "[" + std::to_string(index) + "]";
}

const Json& Field(const Json& object, const char* key, const std::string& path) {
    if (!object.is_object()) {
        Fail(path, "must be an object");
    }
    const auto it = object.find(key);
    if (it == object.end()) {
        Fail(Child(path, key), "is missing");
    }
    return *it;
}

double Number(const Json& value, const std::string& path) {
    if (!value.is_number()) {
        Fail(path, "must be a number");
    }
    return value.get<double>();
}

int Integer(const Json& value, const std::string& path) {
    if (!value.is_number_integer()) {
        Fail(path, "must be an integer");
    }
    if (value.is_number_unsigned() && value.get<uint64_t>() > static_cast<uint64_t>(INT_MAX)) {
        Fail(path, "is out of range");
    }
    const int64_t number = value.get<int64_t>();
    if (number < INT_MIN || number > INT_MAX) {
        Fail(path, "is out of range");
    }
    return static_cast<int>(number);
}

std::string Text(const Json& value, const std::string& path) {
    if (!value.is_string()) {
        Fail(path, "must be a string");
    }
    return value.get<std::string>();
}

bool Boolean(const Json& value, const std::string& path) {
    if (!value.is_boolean()) {
        Fail(path, "must be true or false");
    }
    return value.get<bool>();
}

const Json& Array(const Json& value, const std::string& path) {
    if (!value.is_array()) {
        Fail(path, "must be an array");
    }
    return value;
}

Json NumberOrNull(double value) {
    return std::isfinite(value) ? Json(value) : Json(nullptr);
}

// Checks {"format": expected, "version": 1}
void CheckFormat(const Json& document, const char* expected) {
    const std::string format = Text(Field(document, "format", ""), "format");
    if (format != expected) {
        Fail("format", "is \"" + format + "\", expected \"" + expected + "\"");
    }
    const int version = Integer(Field(document, "version", ""), "version");
    if (version != 1) {
        Fail("version", "is " + std::to_string(version) + "; only version 1 is supported");
    }
}

} // namespace

namespace json_detail {

Json RoiToJson(const Roi& roi) {
    Json shape;
    if (const auto* rectangle = std::get_if<RectangleRoi>(&roi.shape)) {
        shape = {
            {"type", "rectangle"}, {"x", rectangle->x}, {"y", rectangle->y}, {"width", rectangle->width}, {"height", rectangle->height}};
    } else if (const auto* ellipse = std::get_if<EllipseRoi>(&roi.shape)) {
        shape = {{"type", "ellipse"}, {"cx", ellipse->cx}, {"cy", ellipse->cy}, {"rx", ellipse->rx}, {"ry", ellipse->ry},
            {"angle", ellipse->angle_deg}};
    } else {
        const auto& polygon = std::get<PolygonRoi>(roi.shape);
        Json points = Json::array();
        for (const auto& point : polygon.points) {
            points.push_back(Json::array({point[0], point[1]}));
        }
        shape = {{"type", "polygon"}, {"points", points}, {"freehand", polygon.freehand}};
    }
    return {{"id", roi.id}, {"name", roi.name}, {"color", roi.color}, {"shape", shape}};
}

Roi RoiFromJson(const Json& value, const std::string& path) {
    if (!value.is_object()) {
        Fail(path, "must be an object");
    }

    Roi roi;
    if (value.contains("id")) {
        roi.id = Text(value.at("id"), Child(path, "id"));
    }
    if (value.contains("name")) {
        roi.name = Text(value.at("name"), Child(path, "name"));
    }
    if (value.contains("color")) {
        roi.color = Text(value.at("color"), Child(path, "color"));
    }

    const Json& shape = Field(value, "shape", path);
    const std::string shape_path = Child(path, "shape");
    const std::string type = Text(Field(shape, "type", shape_path), Child(shape_path, "type"));
    auto number = [&](const char* key) { return Number(Field(shape, key, shape_path), Child(shape_path, key)); };

    if (type == "rectangle") {
        roi.shape = RectangleRoi{number("x"), number("y"), number("width"), number("height")};
    } else if (type == "ellipse") {
        EllipseRoi ellipse{number("cx"), number("cy"), number("rx"), number("ry"), 0.0};
        if (shape.contains("angle")) {
            ellipse.angle_deg = Number(shape.at("angle"), Child(shape_path, "angle"));
        }
        roi.shape = ellipse;
    } else if (type == "polygon") {
        PolygonRoi polygon;
        const std::string points_path = Child(shape_path, "points");
        const Json& points = Array(Field(shape, "points", shape_path), points_path);
        for (size_t i = 0; i < points.size(); ++i) {
            const std::string point_path = Index(points_path, i);
            const Json& point = points[i];
            if (!point.is_array() || point.size() != 2) {
                Fail(point_path, "must be an [x, y] pair");
            }
            polygon.points.push_back({Number(point[0], Index(point_path, 0)), Number(point[1], Index(point_path, 1))});
        }
        if (shape.contains("freehand")) {
            polygon.freehand = Boolean(shape.at("freehand"), Child(shape_path, "freehand"));
        }
        roi.shape = polygon;
    } else {
        Fail(Child(shape_path, "type"), "must be \"rectangle\", \"ellipse\" or \"polygon\"");
    }
    return roi;
}

Json SettingsToJsonValue(const AnalysisSettings& settings) {
    Json features = Json::array();
    for (const FeatureInfo& info : FeatureCatalog()) {
        if (settings.features.count(info.type) > 0) {
            features.push_back(info.id);
        }
    }
    Json directions = Json::array();
    for (Direction direction : DIRECTIONS_BY_ANGLE) {
        if (settings.directions.count(direction) > 0) {
            directions.push_back(DirectionAngle(direction));
        }
    }

    const QuantizationSettings& quantization = settings.quantization;
    const ScoreSettings& score = settings.score;
    const ScoreCoefficients& c = score.coefficients;

    Json result = Json::object();
    result["features"] = features;
    result["grayLevels"] = settings.gray_levels;
    result["quantization"] = {{"method", QuantizationMethodId(quantization.method)}, {"min", quantization.range_min},
        {"max", quantization.range_max}, {"binWidth", quantization.bin_width}};
    result["distances"] = settings.distances;
    result["directions"] = directions;
    result["aggregation"] = AggregationId(settings.aggregation);
    result["logBase"] = LogBaseId(settings.log_base);
    result["score"] = {{"enabled", score.enabled}, {"age", score.age},
        {"coefficients", Json::array({c.age, c.mean, c.entropy, c.contrast})}, {"profile", ScoreProfileId(score.profile)},
        {"intensityMin", score.intensity_min}, {"intensityMax", score.intensity_max}};
    return result;
}

AnalysisSettings SettingsFromJsonValue(const Json& value, const std::string& path) {
    if (!value.is_object()) {
        Fail(path, "must be an object");
    }

    AnalysisSettings settings;
    const std::string features_path = Child(path, "features");
    const Json& features = Array(Field(value, "features", path), features_path);
    for (size_t i = 0; i < features.size(); ++i) {
        const std::string item_path = Index(features_path, i);
        const std::string id = Text(features[i], item_path);
        const auto type = FeatureTypeFromId(id);
        if (!type) {
            Fail(item_path, "\"" + id + "\" is not a known feature");
        }
        settings.features.insert(*type);
    }

    if (value.contains("grayLevels")) {
        settings.gray_levels = Integer(value.at("grayLevels"), Child(path, "grayLevels"));
    }

    if (value.contains("quantization")) {
        const std::string quantization_path = Child(path, "quantization");
        const Json& quantization = value.at("quantization");
        const std::string method_path = Child(quantization_path, "method");
        const std::string method = Text(Field(quantization, "method", quantization_path), method_path);
        const auto parsed = QuantizationMethodFromId(method);
        if (!parsed) {
            Fail(method_path, "must be \"fixedRange\", \"roiMinMax\", \"fixedBinWidth\" or \"none\"");
        }
        settings.quantization.method = *parsed;
        if (quantization.contains("min")) {
            settings.quantization.range_min = Integer(quantization.at("min"), Child(quantization_path, "min"));
        }
        if (quantization.contains("max")) {
            settings.quantization.range_max = Integer(quantization.at("max"), Child(quantization_path, "max"));
        }
        if (quantization.contains("binWidth")) {
            settings.quantization.bin_width = Number(quantization.at("binWidth"), Child(quantization_path, "binWidth"));
        }
    }

    if (value.contains("distances")) {
        const std::string distances_path = Child(path, "distances");
        const Json& distances = Array(value.at("distances"), distances_path);
        settings.distances.clear();
        for (size_t i = 0; i < distances.size(); ++i) {
            settings.distances.push_back(Integer(distances[i], Index(distances_path, i)));
        }
    }

    if (value.contains("directions")) {
        const std::string directions_path = Child(path, "directions");
        const Json& directions = Array(value.at("directions"), directions_path);
        settings.directions.clear();
        for (size_t i = 0; i < directions.size(); ++i) {
            const std::string item_path = Index(directions_path, i);
            const auto direction = DirectionFromAngle(Integer(directions[i], item_path));
            if (!direction) {
                Fail(item_path, "must be 0, 45, 90 or 135");
            }
            settings.directions.insert(*direction);
        }
    }

    if (value.contains("aggregation")) {
        const std::string aggregation_path = Child(path, "aggregation");
        const auto aggregation = AggregationFromId(Text(value.at("aggregation"), aggregation_path));
        if (!aggregation) {
            Fail(aggregation_path, "must be \"perDirectionAndMean\", \"meanOnly\" or \"meanAndRange\"");
        }
        settings.aggregation = *aggregation;
    }

    if (value.contains("logBase")) {
        const std::string log_base_path = Child(path, "logBase");
        const auto log_base = LogBaseFromId(Text(value.at("logBase"), log_base_path));
        if (!log_base) {
            Fail(log_base_path, "must be \"natural\" or \"log2\"");
        }
        settings.log_base = *log_base;
    }

    if (value.contains("score")) {
        const std::string score_path = Child(path, "score");
        const Json& score = value.at("score");
        if (!score.is_object()) {
            Fail(score_path, "must be an object");
        }
        if (score.contains("enabled")) {
            settings.score.enabled = Boolean(score.at("enabled"), Child(score_path, "enabled"));
        }
        if (score.contains("age")) {
            settings.score.age = Number(score.at("age"), Child(score_path, "age"));
        }
        if (score.contains("coefficients")) {
            const std::string coefficients_path = Child(score_path, "coefficients");
            const Json& coefficients = Array(score.at("coefficients"), coefficients_path);
            if (coefficients.size() != 4) {
                Fail(coefficients_path, "must contain 4 numbers (age, mean, entropy, contrast)");
            }
            settings.score.coefficients = ScoreCoefficients{Number(coefficients[0], Index(coefficients_path, 0)),
                Number(coefficients[1], Index(coefficients_path, 1)), Number(coefficients[2], Index(coefficients_path, 2)),
                Number(coefficients[3], Index(coefficients_path, 3))};
        }
        if (score.contains("profile")) {
            const std::string profile_path = Child(score_path, "profile");
            const auto profile = ScoreProfileFromId(Text(score.at("profile"), profile_path));
            if (!profile) {
                Fail(profile_path, "must be \"calibration\" or \"currentSettings\"");
            }
            settings.score.profile = *profile;
        }
        if (score.contains("intensityMin")) {
            settings.score.intensity_min = Integer(score.at("intensityMin"), Child(score_path, "intensityMin"));
        }
        if (score.contains("intensityMax")) {
            settings.score.intensity_max = Integer(score.at("intensityMax"), Child(score_path, "intensityMax"));
        }
    }

    return settings;
}

Json FeaturesToJson(const Features& features) {
    Json result = Json::object();
    for (Direction direction : DIRECTIONS_BY_ANGLE) {
        result[std::to_string(DirectionAngle(direction))] = NumberOrNull(features.Get(direction));
    }
    result["mean"] = NumberOrNull(features.Avg());
    result["range"] = NumberOrNull(features.Range());
    return result;
}

} // namespace json_detail

std::string RoiSetToJson(const RoiSetDocument& document) {
    Json rois = Json::array();
    for (const Roi& roi : document.rois) {
        rois.push_back(json_detail::RoiToJson(roi));
    }

    Json result = Json::object();
    result["format"] = "glcm-roi-set";
    result["version"] = 1;
    result["image"] = {{"name", document.image.name}, {"width", document.image.width}, {"height", document.image.height},
        {"bitDepth", document.image.bit_depth}, {"sha256", document.image.sha256}};
    result["rois"] = rois;
    return result.dump(2);
}

RoiSetDocument RoiSetFromJson(const std::string& text) {
    try {
        const Json document = Json::parse(text);
        CheckFormat(document, "glcm-roi-set");

        RoiSetDocument result;
        if (document.contains("image")) {
            const Json& image = document.at("image");
            if (!image.is_object()) {
                Fail("image", "must be an object");
            }
            if (image.contains("name")) {
                result.image.name = Text(image.at("name"), "image.name");
            }
            if (image.contains("width")) {
                result.image.width = Integer(image.at("width"), "image.width");
            }
            if (image.contains("height")) {
                result.image.height = Integer(image.at("height"), "image.height");
            }
            if (image.contains("bitDepth")) {
                result.image.bit_depth = Integer(image.at("bitDepth"), "image.bitDepth");
            }
            if (image.contains("sha256")) {
                result.image.sha256 = Text(image.at("sha256"), "image.sha256");
            }
        }

        // Named paths: binding the returned reference while passing temporaries triggers GCC's -Wdangling-reference
        const std::string root_path;
        const std::string rois_path = "rois";
        const Json& rois = Array(Field(document, "rois", root_path), rois_path);
        for (size_t i = 0; i < rois.size(); ++i) {
            result.rois.push_back(json_detail::RoiFromJson(rois[i], Index(rois_path, i)));
        }
        return result;
    } catch (const nlohmann::json::exception& error) {
        throw std::invalid_argument(std::string("Invalid ROI set: ") + error.what());
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::string("Invalid ROI set: ") + error.what());
    }
}

std::string SettingsToJson(const AnalysisSettings& settings) {
    return json_detail::SettingsToJsonValue(settings).dump(2);
}

AnalysisSettings SettingsFromJson(const std::string& text) {
    try {
        return json_detail::SettingsFromJsonValue(Json::parse(text), "settings");
    } catch (const nlohmann::json::exception& error) {
        throw std::invalid_argument(std::string("Invalid analysis settings: ") + error.what());
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::string("Invalid analysis settings: ") + error.what());
    }
}

std::string ResultsToJson(const std::vector<MeasurementResult>& results, const AnalysisSettings& settings, const ExportContext& context) {
    Json items = Json::array();
    for (const MeasurementResult& result : results) {
        Json values = Json::object();
        for (const FeatureInfo& info : FeatureCatalog()) {
            const auto it = result.values.find(info.type);
            if (it != result.values.end()) {
                values[info.id] = json_detail::FeaturesToJson(it->second);
            }
        }

        Json pair_counts = Json::object();
        pair_counts["0"] = result.pair_counts[0];
        pair_counts["45"] = result.pair_counts[3];
        pair_counts["90"] = result.pair_counts[1];
        pair_counts["135"] = result.pair_counts[2];

        Json item = Json::object();
        item["roiId"] = result.roi_id;
        item["roiName"] = result.roi_name;
        item["distance"] = result.distance;
        item["status"] = MeasurementStatusId(result.status);
        item["error"] = result.error;
        item["pixelCount"] = result.pixel_count;
        item["pairCounts"] = pair_counts;
        item["quantization"] = {{"lower", result.quantization_lower}, {"upper", result.quantization_upper}};
        item["values"] = values;
        item["score"] = result.score ? json_detail::FeaturesToJson(*result.score) : Json(nullptr);
        item["warnings"] = result.warnings;
        items.push_back(item);
    }

    Json document = Json::object();
    document["format"] = "glcm-results";
    document["version"] = 1;
    document["coreVersion"] = CORE_VERSION;
    document["timestamp"] = context.timestamp;
    document["image"] = {{"name", context.image_name}, {"sha256", context.image_sha256}};
    document["settings"] = json_detail::SettingsToJsonValue(settings);
    document["results"] = items;
    return document.dump(2);
}

} // namespace glcm
