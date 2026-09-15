#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <opencv2/imgcodecs.hpp>
#include <set>
#include <string>
#include <vector>

#include "io/Identifiers.hpp"
#include "io/Json.hpp"
#include "io/ResultsCsv.hpp"
#include "io/RoiImageExport.hpp"
#include "pipeline/AnalysisRunner.hpp"
#include "pipeline/FeatureCatalog.hpp"
#include "pipeline/Version.hpp"
#include "roi/Roi.hpp"

using namespace glcm;
using nlohmann::json;

namespace {

cv::Mat Pattern8(int rows, int cols) {
    cv::Mat image(rows, cols, CV_8UC1);
    for (int m = 0; m < rows; ++m) {
        for (int n = 0; n < cols; ++n) {
            image.at<uchar>(m, n) = static_cast<uchar>((m * 37 + n * 11 + m * n * 3) % 256);
        }
    }
    return image;
}

Roi MakeRoi(const std::string& id, const std::string& name, RoiShape shape) {
    Roi roi;
    roi.id = id;
    roi.name = name;
    roi.color = "#00C2FF";
    roi.shape = std::move(shape);
    return roi;
}

std::vector<Roi> SampleRois() {
    PolygonRoi freehand;
    freehand.points = {{10.25, 10.5}, {80.125, 20.0}, {40.0, 90.75}};
    freehand.freehand = true;
    return {MakeRoi("7f3c", "ROI 1", RectangleRoi{100.5, 100.0, 64.0, 64.25}),
        MakeRoi("a91e", "ROI 2", EllipseRoi{260.5, 300.0, 40.0, 25.0, 30.0}), MakeRoi("c02d", "ROI 3", freehand)};
}

void ExpectSameRoi(const Roi& a, const Roi& b) {
    EXPECT_EQ(a.id, b.id);
    EXPECT_EQ(a.name, b.name);
    EXPECT_EQ(a.color, b.color);
    ASSERT_EQ(a.shape.index(), b.shape.index());
    if (const auto* rectangle = std::get_if<RectangleRoi>(&a.shape)) {
        const auto& other = std::get<RectangleRoi>(b.shape);
        EXPECT_EQ(rectangle->x, other.x);
        EXPECT_EQ(rectangle->y, other.y);
        EXPECT_EQ(rectangle->width, other.width);
        EXPECT_EQ(rectangle->height, other.height);
    } else if (const auto* ellipse = std::get_if<EllipseRoi>(&a.shape)) {
        const auto& other = std::get<EllipseRoi>(b.shape);
        EXPECT_EQ(ellipse->cx, other.cx);
        EXPECT_EQ(ellipse->cy, other.cy);
        EXPECT_EQ(ellipse->rx, other.rx);
        EXPECT_EQ(ellipse->ry, other.ry);
        EXPECT_EQ(ellipse->angle_deg, other.angle_deg);
    } else {
        const auto& polygon = std::get<PolygonRoi>(a.shape);
        const auto& other = std::get<PolygonRoi>(b.shape);
        EXPECT_EQ(polygon.points, other.points);
        EXPECT_EQ(polygon.freehand, other.freehand);
    }
}

std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

// RFC 4180 fields of one line without embedded line breaks
std::vector<std::string> ParseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quoted) {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') {
                field += '"';
                ++i;
            } else if (c == '"') {
                quoted = false;
            } else {
                field += c;
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            fields.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    fields.push_back(field);
    return fields;
}

struct CsvTable {
    std::vector<std::string> comments;
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;

    size_t Column(const std::string& name) const {
        for (size_t i = 0; i < header.size(); ++i) {
            if (header[i] == name) {
                return i;
            }
        }
        ADD_FAILURE() << "missing column " << name;
        return 0;
    }
};

CsvTable ParseCsv(const std::string& text) {
    CsvTable table;
    for (const std::string& line : Lines(text)) {
        if (line.rfind("# ", 0) == 0) {
            table.comments.push_back(line);
        } else if (table.header.empty()) {
            table.header = ParseCsvLine(line);
        } else {
            table.rows.push_back(ParseCsvLine(line));
        }
    }
    return table;
}

bool Contains(const std::vector<std::string>& items, const std::string& item) {
    for (const std::string& candidate : items) {
        if (candidate == item) {
            return true;
        }
    }
    return false;
}

std::string ErrorOf(const std::function<void()>& action) {
    try {
        action();
    } catch (const std::invalid_argument& error) {
        return error.what();
    }
    return "no exception";
}

std::map<std::string, std::vector<uchar>> ByName(const std::vector<ExportedFile>& files) {
    std::map<std::string, std::vector<uchar>> by_name;
    for (const ExportedFile& file : files) {
        EXPECT_TRUE(by_name.emplace(file.name, file.bytes).second) << "duplicate file " << file.name;
    }
    return by_name;
}

const ExportContext CONTEXT{"pattern.png", "sha-1", "2026-09-14T12:00:00Z", std::nullopt};

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Identifiers
// ---------------------------------------------------------------------------------------------------------------------

TEST(IdentifiersTest, RoundTrip) {
    for (Direction direction : DIRECTIONS_BY_ANGLE) {
        EXPECT_EQ(DirectionFromAngle(DirectionAngle(direction)), direction);
    }
    EXPECT_EQ(DirectionAngle(Direction::RD), 45);
    EXPECT_FALSE(DirectionFromAngle(30).has_value());
    EXPECT_THROW(DirectionAngle(Direction::Avg), std::invalid_argument);

    for (auto method :
        {QuantizationMethod::FixedRange, QuantizationMethod::RoiMinMax, QuantizationMethod::FixedBinWidth, QuantizationMethod::None}) {
        EXPECT_EQ(QuantizationMethodFromId(QuantizationMethodId(method)), method);
    }
    for (auto aggregation : {Aggregation::PerDirectionAndMean, Aggregation::MeanOnly, Aggregation::MeanAndRange}) {
        EXPECT_EQ(AggregationFromId(AggregationId(aggregation)), aggregation);
    }
    EXPECT_EQ(LogBaseFromId("log2"), LogBase::Two);
    EXPECT_EQ(ScoreProfileFromId("currentSettings"), ScoreProfile::CurrentSettings);
    EXPECT_EQ(MeasurementStatusId(MeasurementStatus::Skipped), "skipped");
    EXPECT_FALSE(LogBaseFromId("ln").has_value());
}

// ---------------------------------------------------------------------------------------------------------------------
// ROI set JSON
// ---------------------------------------------------------------------------------------------------------------------

TEST(RoiSetJsonTest, RoundTripsEveryShape) {
    RoiSetDocument document;
    document.image = {"mri16.tif", 512, 512, 16, "abc123"};
    document.rois = SampleRois();

    const std::string text = RoiSetToJson(document);
    const RoiSetDocument parsed = RoiSetFromJson(text);
    EXPECT_EQ(parsed.image.name, "mri16.tif");
    EXPECT_EQ(parsed.image.width, 512);
    EXPECT_EQ(parsed.image.height, 512);
    EXPECT_EQ(parsed.image.bit_depth, 16);
    EXPECT_EQ(parsed.image.sha256, "abc123");
    ASSERT_EQ(parsed.rois.size(), 3u);
    for (size_t i = 0; i < parsed.rois.size(); ++i) {
        ExpectSameRoi(parsed.rois[i], document.rois[i]);
    }

    const json raw = json::parse(text);
    EXPECT_EQ(raw["format"], "glcm-roi-set");
    EXPECT_EQ(raw["version"], 1);
    EXPECT_EQ(raw["rois"][1]["shape"]["type"], "ellipse");
    EXPECT_EQ(raw["rois"][1]["shape"]["angle"], 30.0);
    EXPECT_EQ(raw["rois"][2]["shape"]["points"][0][0], 10.25);
    EXPECT_EQ(raw["rois"][2]["shape"]["freehand"], true);
}

TEST(RoiSetJsonTest, ParsesThePlanExample) {
    const std::string text = R"({
      "format": "glcm-roi-set", "version": 1,
      "image": {"name": "mri16.tif", "width": 512, "height": 512, "bitDepth": 16, "sha256": "x"},
      "rois": [
        {"id": "7f3c", "name": "ROI 1", "color": "#FFD400", "shape": {"type": "rectangle", "x": 100, "y": 100, "width": 64, "height": 64}},
        {"id": "a91e", "name": "ROI 2", "color": "#00C2FF", "shape": {"type": "ellipse", "cx": 260.5, "cy": 300, "rx": 40, "ry": 25, "angle": 30}},
        {"id": "c02d", "name": "ROI 3", "color": "#FF4FD8", "shape": {"type": "polygon", "points": [[10, 10], [80, 20], [40, 90]], "freehand": false}}
      ]})";

    const RoiSetDocument document = RoiSetFromJson(text);
    ASSERT_EQ(document.rois.size(), 3u);
    EXPECT_EQ(std::get<RectangleRoi>(document.rois[0].shape).width, 64.0);
    EXPECT_EQ(std::get<EllipseRoi>(document.rois[1].shape).cx, 260.5);
    EXPECT_EQ(std::get<PolygonRoi>(document.rois[2].shape).points.size(), 3u);
    EXPECT_EQ(document.rois[2].color, "#FF4FD8");
}

TEST(RoiSetJsonTest, ReportsWhereDocumentsAreInvalid) {
    const auto error = [](const std::string& text) { return ErrorOf([&] { RoiSetFromJson(text); }); };

    EXPECT_NE(error("not json").find("Invalid ROI set"), std::string::npos);
    EXPECT_NE(error(R"({"format": "glcm-results", "version": 1, "rois": []})").find("format"), std::string::npos);
    EXPECT_NE(error(R"({"format": "glcm-roi-set", "version": 2, "rois": []})").find("only version 1"), std::string::npos);
    EXPECT_NE(error(R"({"format": "glcm-roi-set", "version": 1})").find("rois is missing"), std::string::npos);
    EXPECT_NE(error(R"({"format": "glcm-roi-set", "version": 1, "rois": [{"shape": {"type": "circle"}}]})").find("rois[0].shape.type"),
        std::string::npos);
    EXPECT_NE(
        error(
            R"({"format": "glcm-roi-set", "version": 1, "rois": [{"shape": {"type": "rectangle", "x": 1, "y": 2, "width": "3", "height": 4}}]})")
            .find("rois[0].shape.width must be a number"),
        std::string::npos);
    EXPECT_NE(error(R"({"format": "glcm-roi-set", "version": 1, "rois": [{"shape": {"type": "polygon", "points": [[1, 2, 3]]}}]})")
                  .find("rois[0].shape.points[0]"),
        std::string::npos);
}

// ---------------------------------------------------------------------------------------------------------------------
// Settings JSON
// ---------------------------------------------------------------------------------------------------------------------

TEST(SettingsJsonTest, RoundTripsAllFields) {
    AnalysisSettings settings = DefaultSettings(16);
    settings.features = {Type::Mean, Type::CorrelationIII, Type::MaximalCorrelationCoefficient};
    settings.gray_levels = 64;
    settings.quantization.method = QuantizationMethod::FixedBinWidth;
    settings.quantization.range_min = 12;
    settings.quantization.range_max = 4000;
    settings.quantization.bin_width = 12.5;
    settings.distances = {4, 1, 2};
    settings.directions = {Direction::V, Direction::RD};
    settings.aggregation = Aggregation::MeanAndRange;
    settings.log_base = LogBase::Two;
    settings.score.enabled = true;
    settings.score.age = 63.5;
    settings.score.coefficients = {1.0, -2.0, 3.25, 4.5};
    settings.score.profile = ScoreProfile::CurrentSettings;
    settings.score.intensity_min = 10;
    settings.score.intensity_max = 4095;

    const AnalysisSettings parsed = SettingsFromJson(SettingsToJson(settings));
    EXPECT_EQ(parsed.features, settings.features);
    EXPECT_EQ(parsed.gray_levels, 64);
    EXPECT_EQ(parsed.quantization.method, QuantizationMethod::FixedBinWidth);
    EXPECT_EQ(parsed.quantization.range_min, 12);
    EXPECT_EQ(parsed.quantization.range_max, 4000);
    EXPECT_EQ(parsed.quantization.bin_width, 12.5);
    EXPECT_EQ(parsed.distances, (std::vector<int>{4, 1, 2}));
    EXPECT_EQ(parsed.directions, settings.directions);
    EXPECT_EQ(parsed.aggregation, Aggregation::MeanAndRange);
    EXPECT_EQ(parsed.log_base, LogBase::Two);
    EXPECT_TRUE(parsed.score.enabled);
    EXPECT_EQ(parsed.score.age, 63.5);
    EXPECT_EQ(parsed.score.coefficients.age, 1.0);
    EXPECT_EQ(parsed.score.coefficients.mean, -2.0);
    EXPECT_EQ(parsed.score.coefficients.entropy, 3.25);
    EXPECT_EQ(parsed.score.coefficients.contrast, 4.5);
    EXPECT_EQ(parsed.score.profile, ScoreProfile::CurrentSettings);
    EXPECT_EQ(parsed.score.intensity_min, 10);
    EXPECT_EQ(parsed.score.intensity_max, 4095);
}

TEST(SettingsJsonTest, ParsesThePlanExampleRequest) {
    const std::string text = R"({
      "features": ["Contrast", "Entropy", "CorrelationII", "CorrelationIII"],
      "grayLevels": 32,
      "quantization": { "method": "fixedRange", "min": 0, "max": 65535 },
      "distances": [1, 2],
      "directions": [0, 45, 90, 135],
      "aggregation": "perDirectionAndMean",
      "logBase": "natural",
      "score": { "enabled": true, "age": 40, "coefficients": [1.138, -1.814, 1.416, 1.714], "profile": "calibration" }
    })";

    const AnalysisSettings settings = SettingsFromJson(text);
    EXPECT_NO_THROW(ValidateSettings(settings));
    EXPECT_EQ(settings.features, (std::set<Type>{Type::Contrast, Type::Entropy, Type::CorrelationII, Type::CorrelationIII}));
    EXPECT_EQ(settings.gray_levels, 32);
    EXPECT_EQ(settings.quantization.range_max, 65535);
    EXPECT_EQ(settings.distances, (std::vector<int>{1, 2}));
    EXPECT_EQ(settings.directions.size(), 4u);
    EXPECT_TRUE(settings.score.enabled);
    EXPECT_EQ(settings.score.coefficients.entropy, 1.416);
    EXPECT_EQ(settings.score.profile, ScoreProfile::Calibration);
}

TEST(SettingsJsonTest, ReportsWhereSettingsAreInvalid) {
    const auto error = [](const std::string& text) { return ErrorOf([&] { SettingsFromJson(text); }); };

    EXPECT_NE(error(R"({"features": ["Contrast", "NoSuchFeature"]})").find("settings.features[1] \"NoSuchFeature\""), std::string::npos);
    EXPECT_NE(error(R"({"grayLevels": 32})").find("settings.features is missing"), std::string::npos);
    EXPECT_NE(error(R"({"features": [], "directions": [30]})").find("settings.directions[0] must be 0, 45, 90 or 135"), std::string::npos);
    EXPECT_NE(error(R"({"features": [], "quantization": {"method": "log"}})").find("settings.quantization.method"), std::string::npos);
    EXPECT_NE(error(R"({"features": [], "score": {"coefficients": [1, 2, 3]}})").find("settings.score.coefficients"), std::string::npos);
    EXPECT_NE(error(R"({"features": [], "grayLevels": 32.5})").find("settings.grayLevels must be an integer"), std::string::npos);
    EXPECT_NE(error("[1, 2]").find("Invalid analysis settings"), std::string::npos);
}

// ---------------------------------------------------------------------------------------------------------------------
// Results JSON and CSV
// ---------------------------------------------------------------------------------------------------------------------

TEST(ResultsJsonTest, WritesEveryMeasurement) {
    AnalysisSettings settings = DefaultSettings(8);
    settings.features = {Type::Mean, Type::Contrast};
    settings.directions = {Direction::H, Direction::V};
    settings.score.enabled = true;

    const AnalysisOutput output = RunAnalysis(
        Pattern8(30, 30), {MakeRoi("r1", "A", RectangleRoi{2, 2, 20, 20}), MakeRoi("r2", "tiny", RectangleRoi{0, 0, 1, 1})}, settings);
    const json document = json::parse(ResultsToJson(output.results, settings, CONTEXT));

    EXPECT_EQ(document["format"], "glcm-results");
    EXPECT_EQ(document["version"], 1);
    EXPECT_EQ(document["coreVersion"], CORE_VERSION);
    EXPECT_EQ(document["timestamp"], "2026-09-14T12:00:00Z");
    EXPECT_EQ(document["image"]["name"], "pattern.png");
    EXPECT_EQ(document["settings"]["grayLevels"], 32);
    ASSERT_EQ(document["results"].size(), 2u);

    const json& ok = document["results"][0];
    const MeasurementResult& measured = output.results[0];
    EXPECT_EQ(ok["status"], "ok");
    EXPECT_EQ(ok["roiName"], "A");
    EXPECT_EQ(ok["distance"], 1);
    EXPECT_EQ(ok["pixelCount"], 400);
    EXPECT_EQ(ok["values"]["Contrast"]["0"].get<double>(), measured.values.at(Type::Contrast).H);
    EXPECT_EQ(ok["values"]["Contrast"]["90"].get<double>(), measured.values.at(Type::Contrast).V);
    EXPECT_TRUE(ok["values"]["Contrast"]["45"].is_null());
    EXPECT_EQ(ok["values"]["Contrast"]["mean"].get<double>(), measured.values.at(Type::Contrast).Avg());
    EXPECT_EQ(ok["values"]["Mean"]["0"].get<double>(), measured.values.at(Type::Mean).H);
    EXPECT_EQ(ok["pairCounts"]["90"], measured.pair_counts[1]);
    EXPECT_EQ(ok["score"]["mean"].get<double>(), measured.score->Avg());

    const json& skipped = document["results"][1];
    EXPECT_EQ(skipped["status"], "skipped");
    EXPECT_FALSE(skipped["error"].get<std::string>().empty());
    EXPECT_TRUE(skipped["score"].is_null());
    EXPECT_TRUE(skipped["values"].empty());
}

TEST(ResultsCsvTest, WritesSettingsHeaderAndRows) {
    AnalysisSettings settings = DefaultSettings(8);
    settings.features = {Type::Mean, Type::CorrelationIII, Type::Contrast};
    settings.directions = {Direction::H, Direction::V};
    settings.score.enabled = true;

    const std::string name = "Region, \"A\"";
    const AnalysisOutput output = RunAnalysis(
        Pattern8(30, 30), {MakeRoi("r1", name, RectangleRoi{2, 2, 20, 20}), MakeRoi("r2", "tiny", RectangleRoi{0, 0, 1, 1})}, settings);
    const std::string csv = ResultsToCsv(output.results, settings, CONTEXT);
    const CsvTable table = ParseCsv(csv);

    EXPECT_TRUE(Contains(table.comments, "# format=glcm-results-csv"));
    EXPECT_TRUE(Contains(table.comments, std::string("# coreVersion=") + CORE_VERSION));
    EXPECT_TRUE(Contains(table.comments, "# grayLevels=32"));
    EXPECT_TRUE(Contains(table.comments, "# quantization=fixedRange [0, 255]"));
    EXPECT_TRUE(Contains(table.comments, "# directions=0;90"));
    EXPECT_TRUE(Contains(table.comments, "# score=enabled"));
    EXPECT_TRUE(Contains(table.comments, "# scoreCoefficients=1.138;-1.814;1.416;1.714"));

    // Feature columns in catalog order, the non-standard one marked
    const size_t mean = table.Column("Mean");
    const size_t contrast = table.Column("Contrast");
    const size_t correlation = table.Column("Correlation III [non-standard]");
    EXPECT_LT(mean, contrast);
    EXPECT_LT(contrast, correlation);
    EXPECT_EQ(table.header.back(), "warnings");
    EXPECT_EQ(table.header[table.header.size() - 2], "Score");

    // Successful ROI: directions 0 and 90, then mean; skipped ROI: one row
    ASSERT_EQ(table.rows.size(), 4u);
    const size_t direction = table.Column("direction");
    const size_t roi_name = table.Column("roiName");
    const size_t status = table.Column("status");
    const MeasurementResult& measured = output.results[0];

    EXPECT_EQ(table.rows[0][roi_name], name);
    EXPECT_NE(csv.find("\"Region, \"\"A\"\"\""), std::string::npos);
    EXPECT_EQ(table.rows[0][direction], "0");
    EXPECT_EQ(std::strtod(table.rows[0][contrast].c_str(), nullptr), measured.values.at(Type::Contrast).H);
    EXPECT_EQ(std::strtod(table.rows[0][mean].c_str(), nullptr), measured.values.at(Type::Mean).H);
    EXPECT_EQ(table.rows[1][direction], "90");
    EXPECT_EQ(std::strtod(table.rows[1][correlation].c_str(), nullptr), measured.values.at(Type::CorrelationIII).V);
    EXPECT_EQ(table.rows[2][direction], "mean");
    EXPECT_EQ(std::strtod(table.rows[2][contrast].c_str(), nullptr), measured.values.at(Type::Contrast).Avg());
    EXPECT_EQ(std::strtod(table.rows[2][table.Column("Score")].c_str(), nullptr), measured.score->Avg());

    EXPECT_EQ(table.rows[3][status], "skipped");
    EXPECT_EQ(table.rows[3][direction], "");
    EXPECT_EQ(table.rows[3][contrast], "");
    EXPECT_NE(table.rows[3].back().find("fewer than 2"), std::string::npos);
    for (const auto& row : table.rows) {
        EXPECT_EQ(row.size(), table.header.size());
    }
}

TEST(ResultsCsvTest, AggregationChoosesRows) {
    AnalysisSettings settings = DefaultSettings(8);
    settings.features = {Type::Contrast};
    const AnalysisOutput output = RunAnalysis(Pattern8(30, 30), {MakeRoi("r1", "A", RectangleRoi{2, 2, 20, 20})}, settings);

    settings.aggregation = Aggregation::MeanOnly;
    CsvTable mean_only = ParseCsv(ResultsToCsv(output.results, settings, CONTEXT));
    ASSERT_EQ(mean_only.rows.size(), 1u);
    EXPECT_EQ(mean_only.rows[0][mean_only.Column("direction")], "mean");

    settings.aggregation = Aggregation::MeanAndRange;
    CsvTable mean_and_range = ParseCsv(ResultsToCsv(output.results, settings, CONTEXT));
    ASSERT_EQ(mean_and_range.rows.size(), 2u);
    EXPECT_EQ(mean_and_range.rows[1][mean_and_range.Column("direction")], "range");
    EXPECT_EQ(std::strtod(mean_and_range.rows[1][mean_and_range.Column("Contrast")].c_str(), nullptr),
        output.results[0].values.at(Type::Contrast).Range());

    settings.aggregation = Aggregation::PerDirectionAndMean;
    EXPECT_EQ(ParseCsv(ResultsToCsv(output.results, settings, CONTEXT)).rows.size(), 5u);
}

TEST(ResultsJsonTest, ReadsBackWhatItWrites) {
    AnalysisSettings settings = DefaultSettings(8);
    settings.features = {Type::Mean, Type::Contrast, Type::CorrelationIII, Type::MaximalCorrelationCoefficient};
    settings.directions = {Direction::H, Direction::V, Direction::RD};
    settings.distances = {1, 3};
    settings.aggregation = Aggregation::MeanAndRange;
    settings.score.enabled = true;

    const AnalysisOutput output = RunAnalysis(Pattern8(30, 30),
        {MakeRoi("r1", "A, \"quoted\"", RectangleRoi{2, 2, 20, 20}), MakeRoi("r2", "tiny", RectangleRoi{0, 0, 1, 1})}, settings);
    const std::string text = ResultsToJson(output.results, settings, CONTEXT);
    const ResultsDocument document = ResultsFromJson(text);

    // Writing the parsed document again gives the same JSON and CSV: nothing is lost
    EXPECT_EQ(ResultsToJson(document.results, document.settings, document.context), text);
    EXPECT_EQ(ResultsToCsv(document.results, document.settings, document.context), ResultsToCsv(output.results, settings, CONTEXT));

    ASSERT_EQ(document.results.size(), 4u);
    const MeasurementResult& original = output.results[0];
    const MeasurementResult& parsed = document.results[0];
    EXPECT_EQ(parsed.roi_name, original.roi_name);
    EXPECT_EQ(parsed.values.at(Type::Contrast).H, original.values.at(Type::Contrast).H);
    EXPECT_EQ(parsed.values.at(Type::Contrast).RD, original.values.at(Type::Contrast).RD);
    EXPECT_TRUE(std::isnan(parsed.values.at(Type::Contrast).LD));
    EXPECT_EQ(parsed.pair_counts, original.pair_counts);
    EXPECT_EQ(parsed.quantization_upper, original.quantization_upper);
    ASSERT_TRUE(parsed.score.has_value());
    EXPECT_EQ(parsed.score->Avg(), original.score->Avg());
    EXPECT_EQ(document.results[2].status, MeasurementStatus::Skipped);
    EXPECT_EQ(document.results[2].error, output.results[2].error);
    EXPECT_EQ(document.context.image_name, CONTEXT.image_name);
    EXPECT_EQ(document.settings.distances, settings.distances);
}

TEST(ResultsCsvTest, WritesAreasWithAPixelSpacing) {
    AnalysisSettings settings = DefaultSettings(8);
    settings.features = {Type::Contrast};
    const AnalysisOutput output =
        RunAnalysis(Pattern8(30, 30), {MakeRoi("r1", "A", RectangleRoi{2, 2, 20, 20}), MakeRoi("r2", "tiny", RectangleRoi{0, 0, 1, 1})}, settings);
    ExportContext context = CONTEXT;
    context.pixel_spacing = PixelSpacing{0.7, 1.25};

    const CsvTable table = ParseCsv(ResultsToCsv(output.results, settings, context));
    EXPECT_TRUE(Contains(table.comments, "# pixelSpacingMm=0.7;1.25"));
    const size_t area = table.Column("areaMm2");
    const size_t pixels = table.Column("pixelCount");
    EXPECT_EQ(area, pixels + 1);
    ASSERT_EQ(table.rows.size(), 6u); // five rows for "A", one for the skipped ROI
    for (const auto& row : table.rows) {
        ASSERT_EQ(row.size(), table.header.size());
        EXPECT_EQ(std::strtod(row[area].c_str(), nullptr), std::stoi(row[pixels]) * 0.7 * 1.25);
    }

    // Without a spacing the file is as before: no comment and no column
    const CsvTable plain = ParseCsv(ResultsToCsv(output.results, settings, CONTEXT));
    EXPECT_EQ(std::find(plain.header.begin(), plain.header.end(), "areaMm2"), plain.header.end());
    EXPECT_TRUE(std::none_of(plain.comments.begin(), plain.comments.end(), [](const std::string& line) { return line.rfind("# pixelSpacing", 0) == 0; }));
}

TEST(ResultsJsonTest, KeepsThePixelSpacing) {
    AnalysisSettings settings = DefaultSettings(8);
    settings.features = {Type::Contrast};
    const AnalysisOutput output = RunAnalysis(Pattern8(30, 30), {MakeRoi("r1", "A", RectangleRoi{2, 2, 20, 20})}, settings);
    ExportContext context = CONTEXT;
    context.pixel_spacing = PixelSpacing{0.703125, 1.3298};

    const std::string text = ResultsToJson(output.results, settings, context);
    EXPECT_EQ(json::parse(text)["image"]["pixelSpacing"], (json{{"x", 0.703125}, {"y", 1.3298}}));
    const ResultsDocument document = ResultsFromJson(text);
    ASSERT_TRUE(document.context.pixel_spacing.has_value());
    EXPECT_EQ(*document.context.pixel_spacing, *context.pixel_spacing);
    EXPECT_EQ(ResultsToJson(document.results, document.settings, document.context), text);
    EXPECT_EQ(ResultsToCsv(document.results, document.settings, document.context), ResultsToCsv(output.results, settings, context));
    EXPECT_FALSE(ResultsFromJson(ResultsToJson(output.results, settings, CONTEXT)).context.pixel_spacing.has_value());

    json invalid = json::parse(text);
    invalid["image"]["pixelSpacing"]["y"] = 0;
    EXPECT_NE(ErrorOf([&] { ResultsFromJson(invalid.dump()); }).find("image.pixelSpacing.y must be positive"), std::string::npos);
    invalid["image"]["pixelSpacing"] = "0.7 mm";
    EXPECT_NE(ErrorOf([&] { ResultsFromJson(invalid.dump()); }).find("image.pixelSpacing must be an object"), std::string::npos);
    invalid["image"]["pixelSpacing"] = json{{"x", 1}};
    EXPECT_NE(ErrorOf([&] { ResultsFromJson(invalid.dump()); }).find("image.pixelSpacing.y"), std::string::npos);
}

TEST(ResultsJsonTest, ReportsWhereResultsAreInvalid) {
    auto message = [](const std::string& text) {
        try {
            ResultsFromJson(text);
        } catch (const std::invalid_argument& error) {
            return std::string(error.what());
        }
        return std::string("no error");
    };
    const std::string prefix = R"({"format": "glcm-results", "version": 1, "settings": {"features": ["Contrast"]}, "results": [)";

    EXPECT_NE(message("{").find("Invalid results"), std::string::npos);
    EXPECT_NE(message(R"({"format": "glcm-roi-set", "version": 1})").find("format"), std::string::npos);
    EXPECT_NE(message(R"({"format": "glcm-results", "version": 1, "results": []})").find("settings is missing"), std::string::npos);
    EXPECT_NE(
        message(prefix + R"({"roiId": "a", "roiName": "A", "distance": 1, "status": "done", "pixelCount": 4}]})").find("results[0].status"),
        std::string::npos);
    EXPECT_NE(
        message(prefix + R"({"roiId": "a", "roiName": "A", "distance": 1, "status": "ok", "pixelCount": 4, "values": {"Nope": {}}}]})")
            .find("results[0].values.Nope"),
        std::string::npos);
    EXPECT_NE(message(prefix + R"({"roiId": "a", "status": "ok"}]})").find("results[0].roiName is missing"), std::string::npos);

    const ResultsDocument minimal = ResultsFromJson(prefix + "]}");
    EXPECT_TRUE(minimal.results.empty());
    EXPECT_EQ(minimal.settings.features, std::set<Type>{Type::Contrast});
}

TEST(ResultsCsvTest, PrefixesFormulaTextButNotNumbers) {
    AnalysisSettings settings = DefaultSettings(8);
    settings.features = {Type::Contrast, Type::InformationMeasuresOfCorrelationI};
    const std::vector<std::string> formulas = {"=HYPERLINK(\"http://example.org\",\"x\")", "+1", "-left", "@SUM(A1)", "\tindented"};
    std::vector<Roi> rois;
    for (size_t i = 0; i < formulas.size(); ++i) {
        rois.push_back(MakeRoi("r" + std::to_string(i), formulas[i], RectangleRoi{2, 2, 20, 20}));
    }
    rois.push_back(MakeRoi("-id", "a=b", RectangleRoi{2, 2, 20, 20}));
    const AnalysisOutput output = RunAnalysis(Pattern8(30, 30), rois, settings);
    const ExportContext context{"=calc.png", "sha-1", "2026-09-14T12:00:00Z", std::nullopt};
    const CsvTable table = ParseCsv(ResultsToCsv(output.results, settings, context));

    std::string imc_header;
    for (const glcm::FeatureInfo& info : glcm::FeatureCatalog()) {
        if (info.type == Type::InformationMeasuresOfCorrelationI) {
            imc_header = info.non_standard ? info.name + " [non-standard]" : info.name;
        }
    }
    const size_t roi_name = table.Column("roiName");
    const size_t roi_id = table.Column("roiId");
    const size_t image = table.Column("image");
    const size_t imc = table.Column(imc_header);

    // Four directions and the mean per ROI
    ASSERT_EQ(table.rows.size(), rois.size() * 5);
    bool negative = false;
    for (size_t r = 0; r < table.rows.size(); ++r) {
        const auto& row = table.rows[r];
        const Roi& roi = rois[r / 5];
        const bool formula = std::string("=+-@\t").find(roi.name[0]) != std::string::npos;
        EXPECT_EQ(row[roi_name], formula ? "'" + roi.name : roi.name);
        EXPECT_EQ(row[roi_id], roi.id == "-id" ? "'-id" : roi.id);
        EXPECT_EQ(row[image], "'=calc.png");
        // Numbers are never prefixed, including negative ones (IMC1 is at most 0)
        ASSERT_FALSE(row[imc].empty());
        EXPECT_NE(row[imc][0], '\'');
        negative = negative || std::strtod(row[imc].c_str(), nullptr) < 0.0;
    }
    EXPECT_TRUE(negative);
}

// ---------------------------------------------------------------------------------------------------------------------
// ROI image export
// ---------------------------------------------------------------------------------------------------------------------

TEST(RoiImageExportTest, ExportsCropsMasksQuantizedImagesAndManifest) {
    const cv::Mat image = Pattern8(40, 40);
    const EllipseRoi ellipse{25.0, 25.0, 6.0, 4.0, 0.0};
    const std::vector<Roi> rois = {MakeRoi("r1", "a/b", RectangleRoi{2, 3, 10, 8}), MakeRoi("r2", "a/b", ellipse),
        MakeRoi("r3", "outside", RectangleRoi{100, 100, 5, 5})};
    RoiImageExportOptions options;
    options.include_quantized = true;

    const auto files = ByName(ExportRoiImages(image, rois, DefaultSettings(8), options));
    EXPECT_EQ(files.size(), 7u);
    for (const char* name : {"a_b.png", "a_b_mask.png", "a_b_q32.png", "a_b_2.png", "a_b_2_mask.png", "a_b_2_q32.png", "manifest.json"}) {
        EXPECT_EQ(files.count(name), 1u) << name;
    }

    const cv::Mat rectangle = cv::imdecode(files.at("a_b.png"), cv::IMREAD_UNCHANGED);
    ASSERT_EQ(rectangle.size(), cv::Size(10, 8));
    EXPECT_EQ(cv::countNonZero(rectangle != image(cv::Rect(2, 3, 10, 8))), 0);

    const cv::Mat full_mask = RasterizeMask(ellipse, image.size());
    const cv::Rect box = MaskBoundingBox(full_mask);
    const cv::Mat crop = cv::imdecode(files.at("a_b_2.png"), cv::IMREAD_UNCHANGED);
    const cv::Mat mask = cv::imdecode(files.at("a_b_2_mask.png"), cv::IMREAD_UNCHANGED);
    ASSERT_EQ(crop.size(), box.size());
    EXPECT_EQ(cv::countNonZero(mask != full_mask(box)), 0);
    for (int row = 0; row < crop.rows; ++row) {
        for (int col = 0; col < crop.cols; ++col) {
            const uchar expected = mask.at<uchar>(row, col) ? image.at<uchar>(box.y + row, box.x + col) : 0;
            ASSERT_EQ(crop.at<uchar>(row, col), expected) << row << "," << col;
        }
    }

    const cv::Mat quantized = cv::imdecode(files.at("a_b_2_q32.png"), cv::IMREAD_UNCHANGED);
    double max_level = 0.0;
    cv::minMaxLoc(quantized, nullptr, &max_level);
    EXPECT_LT(max_level, 32.0);

    const auto& manifest_bytes = files.at("manifest.json");
    const json manifest = json::parse(std::string(manifest_bytes.begin(), manifest_bytes.end()));
    EXPECT_EQ(manifest["format"], "glcm-roi-images");
    ASSERT_EQ(manifest["entries"].size(), 3u);
    EXPECT_EQ(manifest["entries"][0]["boundingBox"]["width"], 10);
    EXPECT_EQ(manifest["entries"][0]["pixelCount"], 80);
    EXPECT_EQ(manifest["entries"][1]["image"], "a_b_2.png");
    EXPECT_EQ(manifest["entries"][1]["quantized"], "a_b_2_q32.png");
    EXPECT_EQ(manifest["entries"][2]["skipped"], true);
    EXPECT_EQ(manifest["settings"]["grayLevels"], 32);
}

TEST(RoiImageExportTest, TransparentAndSixteenBitOutputs) {
    const cv::Mat image = Pattern8(30, 30);
    const EllipseRoi ellipse{15.0, 15.0, 8.0, 5.0, 20.0};
    RoiImageExportOptions transparent;
    transparent.transparent_outside = true;

    auto files = ByName(ExportRoiImages(image, {MakeRoi("id", "ROI 1", ellipse)}, DefaultSettings(8), transparent));
    const cv::Mat bgra = cv::imdecode(files.at("ROI_1.png"), cv::IMREAD_UNCHANGED);
    ASSERT_EQ(bgra.channels(), 4);
    const cv::Mat full_mask = RasterizeMask(ellipse, image.size());
    const cv::Rect box = MaskBoundingBox(full_mask);
    cv::Mat alpha;
    cv::extractChannel(bgra, alpha, 3);
    EXPECT_EQ(cv::countNonZero(alpha != full_mask(box)), 0);

    cv::Mat sixteen(30, 30, CV_16UC1);
    for (int m = 0; m < 30; ++m) {
        for (int n = 0; n < 30; ++n) {
            sixteen.at<uint16_t>(m, n) = static_cast<uint16_t>(1000 + m * 300 + n * 7);
        }
    }
    files = ByName(ExportRoiImages(sixteen, {MakeRoi("", "", RectangleRoi{5, 5, 10, 10})}, DefaultSettings(16)));
    EXPECT_EQ(files.count("roi.tif"), 1u);
    const cv::Mat decoded = cv::imdecode(files.at("roi.tif"), cv::IMREAD_UNCHANGED);
    ASSERT_EQ(decoded.type(), CV_16UC1);
    EXPECT_EQ(cv::countNonZero(decoded != sixteen(cv::Rect(5, 5, 10, 10))), 0);
}

TEST(RoiImageExportTest, SanitizesFileNames) {
    EXPECT_EQ(SanitizeFileName("ROI 1"), "ROI_1");
    EXPECT_EQ(SanitizeFileName("../etc/passwd"), "_._etc_passwd");
    EXPECT_EQ(SanitizeFileName(""), "roi");
    EXPECT_EQ(SanitizeFileName("R\xC3\xA9gion"), "R__gion");
    EXPECT_EQ(SanitizeFileName(std::string(150, 'a')).size(), 100u);
    EXPECT_EQ(SanitizeFileName("tumor-2_left.v1"), "tumor-2_left.v1");
}

TEST(RoiImageExportTest, DerivedFileNamesNeverCollide) {
    const cv::Mat image = Pattern8(40, 40);
    const RectangleRoi square{2, 2, 6, 6};
    // ROI names equal to another ROI's mask or quantized file name, in both orders
    const std::vector<Roi> rois = {MakeRoi("1", "a", square), MakeRoi("2", "a_mask", square), MakeRoi("3", "b_mask", square),
        MakeRoi("4", "b", square), MakeRoi("5", "x", square), MakeRoi("6", "x_q32", square), MakeRoi("7", "manifest.json", square)};
    RoiImageExportOptions options;
    options.include_quantized = true;

    const std::vector<ExportedFile> files = ExportRoiImages(image, rois, DefaultSettings(8), options);
    std::set<std::string> names;
    for (const ExportedFile& file : files) {
        EXPECT_TRUE(names.insert(file.name).second) << "duplicate file name " << file.name;
    }
    EXPECT_EQ(files.size(), rois.size() * 3 + 1);
    for (const char* name : {"a.png", "a_mask.png", "a_mask_2.png", "a_mask_2_mask.png", "b_mask.png", "b_mask_mask.png", "b_2.png",
             "b_2_mask.png", "x_q32.png", "x_q32_2.png", "x_q32_2_q32.png", "manifest.json.png", "manifest.json"}) {
        EXPECT_EQ(names.count(name), 1u) << name;
    }
}
