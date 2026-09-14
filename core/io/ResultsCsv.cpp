#include "io/ResultsCsv.hpp"

#include <charconv>
#include <cmath>
#include <functional>
#include <sstream>

#include "io/Identifiers.hpp"
#include "pipeline/FeatureCatalog.hpp"
#include "pipeline/Version.hpp"

namespace glcm {

namespace {

std::string Escape(const std::string& field) {
    if (field.find_first_of(",\"\n\r") == std::string::npos) {
        return field;
    }
    std::string quoted = "\"";
    for (char c : field) {
        if (c == '"') {
            quoted += "\"\"";
        } else {
            quoted += c;
        }
    }
    return quoted + "\"";
}

// Keeps a comment value on one line
std::string OneLine(std::string text) {
    for (char& c : text) {
        if (c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    return text;
}

std::string FormatNumber(double value) {
    if (!std::isfinite(value)) {
        return "";
    }
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
}

std::string Join(const std::vector<std::string>& items, const std::string& separator) {
    std::string joined;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            joined += separator;
        }
        joined += items[i];
    }
    return joined;
}

void WriteRow(std::ostringstream& out, const std::vector<std::string>& fields) {
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        out << Escape(fields[i]);
    }
    out << '\n';
}

std::string QuantizationDescription(const QuantizationSettings& quantization) {
    switch (quantization.method) {
        case QuantizationMethod::FixedRange:
            return "fixedRange [" + std::to_string(quantization.range_min) + ", " + std::to_string(quantization.range_max) + "]";
        case QuantizationMethod::FixedBinWidth:
            return "fixedBinWidth " + FormatNumber(quantization.bin_width);
        default:
            return QuantizationMethodId(quantization.method);
    }
}

struct RowKind {
    std::string label;
    std::function<double(const Features&)> value;
};

std::vector<RowKind> RowKinds(const AnalysisSettings& settings) {
    const RowKind mean{"mean", [](const Features& features) { return features.Avg(); }};
    std::vector<RowKind> kinds;
    switch (settings.aggregation) {
        case Aggregation::PerDirectionAndMean:
            for (Direction direction : DIRECTIONS_BY_ANGLE) {
                if (settings.directions.count(direction) > 0) {
                    kinds.push_back({std::to_string(DirectionAngle(direction)),
                        [direction](const Features& features) { return features.Get(direction); }});
                }
            }
            kinds.push_back(mean);
            break;
        case Aggregation::MeanOnly:
            kinds.push_back(mean);
            break;
        case Aggregation::MeanAndRange:
            kinds.push_back(mean);
            kinds.push_back({"range", [](const Features& features) { return features.Range(); }});
            break;
    }
    return kinds;
}

} // namespace

std::string ResultsToCsv(const std::vector<MeasurementResult>& results, const AnalysisSettings& settings, const ExportContext& context) {
    std::ostringstream out;

    std::vector<std::string> distances;
    for (int distance : settings.distances) {
        distances.push_back(std::to_string(distance));
    }
    std::vector<std::string> angles;
    for (Direction direction : DIRECTIONS_BY_ANGLE) {
        if (settings.directions.count(direction) > 0) {
            angles.push_back(std::to_string(DirectionAngle(direction)));
        }
    }

    out << "# format=glcm-results-csv\n";
    out << "# version=1\n";
    out << "# coreVersion=" << CORE_VERSION << "\n";
    out << "# timestamp=" << OneLine(context.timestamp) << "\n";
    out << "# image=" << OneLine(context.image_name) << "\n";
    out << "# imageSha256=" << OneLine(context.image_sha256) << "\n";
    out << "# grayLevels=" << settings.gray_levels << "\n";
    out << "# quantization=" << QuantizationDescription(settings.quantization) << "\n";
    out << "# distances=" << Join(distances, ";") << "\n";
    out << "# directions=" << Join(angles, ";") << "\n";
    out << "# aggregation=" << AggregationId(settings.aggregation) << "\n";
    out << "# logBase=" << LogBaseId(settings.log_base) << "\n";
    if (settings.score.enabled) {
        const ScoreCoefficients& c = settings.score.coefficients;
        out << "# score=enabled\n";
        out << "# scoreAge=" << FormatNumber(settings.score.age) << "\n";
        out << "# scoreCoefficients="
            << Join({FormatNumber(c.age), FormatNumber(c.mean), FormatNumber(c.entropy), FormatNumber(c.contrast)}, ";") << "\n";
        out << "# scoreProfile=" << ScoreProfileId(settings.score.profile) << "\n";
    } else {
        out << "# score=disabled\n";
    }

    std::vector<const FeatureInfo*> columns;
    for (const FeatureInfo& info : FeatureCatalog()) {
        if (settings.features.count(info.type) > 0) {
            columns.push_back(&info);
        }
    }

    std::vector<std::string> header = {"timestamp", "image", "imageSha256", "roiName", "roiId", "status", "pixelCount", "grayLevels",
        "quantization", "distance", "direction"};
    for (const FeatureInfo* info : columns) {
        header.push_back(info->non_standard ? info->name + " [non-standard]" : info->name);
    }
    if (settings.score.enabled) {
        header.push_back("Score");
    }
    header.push_back("warnings");
    WriteRow(out, header);

    const std::string quantization = QuantizationDescription(settings.quantization);
    const std::vector<RowKind> kinds = RowKinds(settings);

    for (const MeasurementResult& result : results) {
        const std::vector<std::string> common = {context.timestamp, context.image_name, context.image_sha256, result.roi_name,
            result.roi_id, MeasurementStatusId(result.status), std::to_string(result.pixel_count), std::to_string(settings.gray_levels),
            quantization, std::to_string(result.distance)};

        std::vector<std::string> notes = result.warnings;
        if (!result.error.empty()) {
            notes.insert(notes.begin(), result.error);
        }

        if (result.status != MeasurementStatus::Ok) {
            std::vector<std::string> row = common;
            row.push_back("");
            row.insert(row.end(), columns.size() + (settings.score.enabled ? 1 : 0), "");
            row.push_back(Join(notes, " | "));
            WriteRow(out, row);
            continue;
        }

        for (const RowKind& kind : kinds) {
            std::vector<std::string> row = common;
            row.push_back(kind.label);
            for (const FeatureInfo* info : columns) {
                const auto it = result.values.find(info->type);
                row.push_back(it == result.values.end() ? "" : FormatNumber(kind.value(it->second)));
            }
            if (settings.score.enabled) {
                row.push_back(result.score ? FormatNumber(kind.value(*result.score)) : "");
            }
            row.push_back(Join(notes, " | "));
            WriteRow(out, row);
        }
    }

    return out.str();
}

} // namespace glcm
