#include <gtest/gtest.h>

#include <opencv2/core.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "io/Json.hpp"
#include "io/ResultsCsv.hpp"
#include "pipeline/AnalysisRunner.hpp"
#include "pipeline/AnalysisSettings.hpp"
#include "roi/Roi.hpp"

using namespace glcm;

namespace {

Roi MakeRoi(const std::string& id, const std::string& name, RoiShape shape, const std::string& class_name = "") {
    Roi roi;
    roi.id = id;
    roi.name = name;
    roi.color = "#FFD400";
    roi.shape = shape;
    roi.class_name = class_name;
    return roi;
}

cv::Mat Pattern8(int rows, int cols) {
    cv::Mat image(rows, cols, CV_8UC1);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            image.at<uchar>(y, x) = static_cast<uchar>((x * 37 + y * 11 + x * y * 3) % 256);
        }
    }
    return image;
}

std::string FirstLineStartingWith(const std::string& text, const std::string& prefix) {
    size_t start = 0;
    while (start < text.size()) {
        const size_t end = text.find('\n', start);
        const std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (line.rfind(prefix, 0) == 0) {
            return line;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return "";
}

} // namespace

TEST(RoiClassTest, RoiSetsKeepTheClassesAndStayUnchangedWithoutThem) {
    RoiSetDocument document;
    document.image.name = "image.png";
    document.classes = {{"lesion", "#FF3B3B"}, {"normal", "#39FF6A"}};
    document.rois = {MakeRoi("a", "A", RectangleRoi{1, 1, 3, 3}, "lesion"), MakeRoi("b", "B", EllipseRoi{5, 5, 2, 2, 0})};

    const std::string json = RoiSetToJson(document);
    const RoiSetDocument parsed = RoiSetFromJson(json);
    ASSERT_EQ(parsed.classes.size(), 2u);
    EXPECT_EQ(parsed.classes[1].name, "normal");
    EXPECT_EQ(parsed.classes[1].color, "#39FF6A");
    EXPECT_EQ(parsed.rois[0].class_name, "lesion");
    EXPECT_EQ(parsed.rois[1].class_name, "");
    EXPECT_EQ(RoiSetToJson(parsed), json);

    RoiSetDocument plain = document;
    plain.classes.clear();
    plain.rois[0].class_name.clear();
    const std::string plain_json = RoiSetToJson(plain);
    EXPECT_EQ(plain_json.find("class"), std::string::npos);

    const auto invalid = [](const std::string& text) { EXPECT_THROW(RoiSetFromJson(text), std::invalid_argument) << text; };
    invalid(R"({"format": "glcm-roi-set", "version": 1, "classes": [{"name": ""}], "rois": []})");
    invalid(R"({"format": "glcm-roi-set", "version": 1, "classes": [{"name": "a"}, {"name": "a"}], "rois": []})");
    invalid(
        R"({"format": "glcm-roi-set", "version": 1, "rois": [{"class": "", "shape": {"type": "rectangle", "x": 0, "y": 0, "width": 2, "height": 2}}]})");
    invalid(
        R"({"format": "glcm-roi-set", "version": 1, "rois": [{"class": 3, "shape": {"type": "rectangle", "x": 0, "y": 0, "width": 2, "height": 2}}]})");
}

TEST(RoiClassTest, ResultsCarryTheRoiClassIntoJsonAndCsv) {
    const cv::Mat gray = Pattern8(32, 32);
    const std::vector<Roi> rois = {
        MakeRoi("a", "A", RectangleRoi{2, 2, 12, 12}, "lesion, early"), MakeRoi("b", "B", RectangleRoi{16, 16, 12, 12})};
    AnalysisSettings settings = DefaultSettings(8);
    settings.features = {Type::Contrast};
    settings.aggregation = Aggregation::MeanOnly;
    const AnalysisOutput output = RunAnalysis(gray, rois, settings);
    ASSERT_EQ(output.results.size(), 2u);
    EXPECT_EQ(output.results[0].roi_class, "lesion, early");
    EXPECT_EQ(output.results[1].roi_class, "");

    ExportContext context;
    context.image_name = "pattern.png";
    context.timestamp = "2026-09-15T12:00:00Z";
    const std::string csv = ResultsToCsv(output.results, settings, context);
    const std::string header = FirstLineStartingWith(csv, "timestamp,");
    EXPECT_NE(header.find(",roiName,roiId,roiClass,status,"), std::string::npos) << header;
    EXPECT_NE(csv.find(",A,a,\"lesion, early\",ok,"), std::string::npos) << csv;
    EXPECT_NE(csv.find(",B,b,,ok,"), std::string::npos) << csv;

    const std::string json = ResultsToJson(output.results, settings, context);
    const ResultsDocument document = ResultsFromJson(json);
    EXPECT_EQ(document.results[0].roi_class, "lesion, early");
    EXPECT_EQ(ResultsToJson(document.results, document.settings, document.context), json);
    EXPECT_EQ(ResultsToCsv(document.results, document.settings, document.context), csv);

    // Without classes the files have no class column or field
    std::vector<MeasurementResult> plain = output.results;
    plain[0].roi_class.clear();
    EXPECT_EQ(FirstLineStartingWith(ResultsToCsv(plain, settings, context), "timestamp,").find("roiClass"), std::string::npos);
    EXPECT_EQ(ResultsToJson(plain, settings, context).find("roiClass"), std::string::npos);
}
