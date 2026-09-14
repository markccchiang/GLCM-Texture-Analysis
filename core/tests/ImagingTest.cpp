#include <gtest/gtest.h>
#include <unistd.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "imaging/DisplayRenderer.hpp"
#include "imaging/ImageLoader.hpp"
#include "imaging/Quantizer.hpp"

using glcm::QuantizationMethod;
using glcm::QuantizationSettings;
using glcm::Quantize;

namespace {

namespace fs = std::filesystem;

cv::Mat FullMask(const cv::Mat& image) {
    return cv::Mat(image.size(), CV_8UC1, cv::Scalar(255));
}

// 1 x 256 image with the values 0 ... 255
cv::Mat Gradient8() {
    cv::Mat image(1, 256, CV_8UC1);
    for (int v = 0; v < 256; ++v) {
        image.at<uchar>(0, v) = static_cast<uchar>(v);
    }
    return image;
}

QuantizationSettings Settings(QuantizationMethod method, int range_min = 0, int range_max = 255, double bin_width = 1.0) {
    QuantizationSettings settings;
    settings.method = method;
    settings.range_min = range_min;
    settings.range_max = range_max;
    settings.bin_width = bin_width;
    return settings;
}

// Unique path in the temporary directory, removed when the object goes out of scope
struct TemporaryFile {
    explicit TemporaryFile(const std::string& name)
        : path(fs::temp_directory_path() / ("glcm_test_" + std::to_string(getpid()) + "_" + name)) {}
    ~TemporaryFile() {
        std::error_code ignored;
        fs::remove(path, ignored);
    }
    fs::path path;
};

bool ImagesEqual(const cv::Mat& a, const cv::Mat& b) {
    if (a.size() != b.size() || a.type() != b.type()) {
        return false;
    }
    cv::Mat difference = a != b;
    return cv::countNonZero(difference.reshape(1)) == 0;
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Quantizer
// ---------------------------------------------------------------------------------------------------------------------

TEST(QuantizerTest, FixedRangeOver256LevelsIsIdentity) {
    const cv::Mat image = Gradient8();
    const auto result = Quantize(image, FullMask(image), 256, Settings(QuantizationMethod::FixedRange, 0, 255));
    EXPECT_TRUE(ImagesEqual(result.image, image));
    EXPECT_EQ(result.lower, 0);
    EXPECT_EQ(result.upper, 255);
    EXPECT_EQ(result.pixels, 256);
}

TEST(QuantizerTest, FixedRangeTo32LevelsDividesBy8) {
    const cv::Mat image = Gradient8();
    const auto result = Quantize(image, FullMask(image), 32, Settings(QuantizationMethod::FixedRange, 0, 255));
    for (int v = 0; v < 256; ++v) {
        EXPECT_EQ(result.image.at<uchar>(0, v), v / 8) << "value " << v;
    }
}

TEST(QuantizerTest, FixedRangeOn16BitImage) {
    const std::vector<uint16_t> values = {0, 1, 2047, 2048, 4095, 30000, 65534, 65535};
    cv::Mat image(1, static_cast<int>(values.size()), CV_16UC1);
    for (size_t i = 0; i < values.size(); ++i) {
        image.at<uint16_t>(0, static_cast<int>(i)) = values[i];
    }
    const auto result = Quantize(image, FullMask(image), 32, Settings(QuantizationMethod::FixedRange, 0, 65535));
    for (size_t i = 0; i < values.size(); ++i) {
        EXPECT_EQ(result.image.at<uchar>(0, static_cast<int>(i)), values[i] / 2048) << "value " << values[i];
    }
}

TEST(QuantizerTest, FixedRangeClipsValuesOutsideTheRange) {
    const cv::Mat image = Gradient8();
    const auto result = Quantize(image, FullMask(image), 10, Settings(QuantizationMethod::FixedRange, 100, 199));
    EXPECT_EQ(result.image.at<uchar>(0, 0), 0);
    EXPECT_EQ(result.image.at<uchar>(0, 100), 0);
    EXPECT_EQ(result.image.at<uchar>(0, 150), 5); // floor(50 * 10 / 100)
    EXPECT_EQ(result.image.at<uchar>(0, 199), 9);
    EXPECT_EQ(result.image.at<uchar>(0, 255), 9);
}

TEST(QuantizerTest, RoiMinMaxUsesOnlyMaskPixels) {
    const cv::Mat image = Gradient8();
    cv::Mat mask = cv::Mat::zeros(image.size(), CV_8UC1);
    mask(cv::Rect(10, 0, 31, 1)).setTo(cv::Scalar(255)); // values 10 ... 40

    const auto result = Quantize(image, mask, 8, Settings(QuantizationMethod::RoiMinMax));
    EXPECT_EQ(result.lower, 10);
    EXPECT_EQ(result.upper, 40);
    EXPECT_EQ(result.pixels, 31);
    EXPECT_EQ(result.image.at<uchar>(0, 10), 0);
    EXPECT_EQ(result.image.at<uchar>(0, 40), 7);
    EXPECT_EQ(result.image.at<uchar>(0, 25), 3);  // floor(15 * 8 / 31)
    EXPECT_EQ(result.image.at<uchar>(0, 200), 0); // outside the mask
}

TEST(QuantizerTest, ConstantRoiMapsToLevelZero) {
    const cv::Mat image(6, 6, CV_16UC1, cv::Scalar(1234));
    const auto result = Quantize(image, FullMask(image), 32, Settings(QuantizationMethod::RoiMinMax));
    EXPECT_EQ(cv::countNonZero(result.image), 0);
    EXPECT_EQ(result.lower, 1234);
    EXPECT_EQ(result.upper, 1234);
}

TEST(QuantizerTest, FixedBinWidthStartsAtRoiMinimum) {
    const cv::Mat image = Gradient8();
    cv::Mat mask = cv::Mat::zeros(image.size(), CV_8UC1);
    mask(cv::Rect(10, 0, 25, 1)).setTo(cv::Scalar(255)); // values 10 ... 34

    const auto result = Quantize(image, mask, 8, Settings(QuantizationMethod::FixedBinWidth, 0, 0, 5.0));
    EXPECT_EQ(result.image.at<uchar>(0, 10), 0);
    EXPECT_EQ(result.image.at<uchar>(0, 14), 0);
    EXPECT_EQ(result.image.at<uchar>(0, 15), 1);
    EXPECT_EQ(result.image.at<uchar>(0, 34), 4);

    // 25 intensities with width 1 need 25 levels, more than Ng = 8
    EXPECT_THROW(Quantize(image, mask, 8, Settings(QuantizationMethod::FixedBinWidth, 0, 0, 1.0)), std::invalid_argument);
}

TEST(QuantizerTest, NoneKeepsValuesBelowNg) {
    cv::Mat image = Gradient8()(cv::Rect(0, 0, 16, 1)).clone(); // values 0 ... 15
    const auto result = Quantize(image, FullMask(image), 16, Settings(QuantizationMethod::None));
    EXPECT_TRUE(ImagesEqual(result.image, image));

    image.at<uchar>(0, 3) = 16;
    EXPECT_THROW(Quantize(image, FullMask(image), 16, Settings(QuantizationMethod::None)), std::invalid_argument);
}

TEST(QuantizerTest, InvalidInputsThrow) {
    const cv::Mat image = Gradient8();
    const cv::Mat mask = FullMask(image);
    const auto fixed = Settings(QuantizationMethod::FixedRange);

    EXPECT_THROW(Quantize(image, mask, 1, fixed), std::invalid_argument);
    EXPECT_THROW(Quantize(image, mask, 257, fixed), std::invalid_argument);
    EXPECT_THROW(Quantize(image, mask, 8, Settings(QuantizationMethod::FixedRange, 200, 100)), std::invalid_argument);
    EXPECT_THROW(Quantize(image, mask, 8, Settings(QuantizationMethod::FixedBinWidth, 0, 0, 0.0)), std::invalid_argument);
    EXPECT_THROW(Quantize(image, cv::Mat::zeros(2, 2, CV_8UC1), 8, fixed), std::invalid_argument);
    EXPECT_THROW(Quantize(cv::Mat::zeros(1, 256, CV_32FC1), mask, 8, fixed), std::invalid_argument);
    EXPECT_THROW(Quantize(image, cv::Mat::zeros(image.size(), CV_8UC1), 8, Settings(QuantizationMethod::RoiMinMax)), std::invalid_argument);
}

// ---------------------------------------------------------------------------------------------------------------------
// DisplayRenderer
// ---------------------------------------------------------------------------------------------------------------------

TEST(DisplayRendererTest, WindowLevelMatchesRoundedFormula) {
    // round-half-up of (v - min) * 255 / (max - min) computed in double; exact for these integer inputs
    auto reference = [](int value, int window_min, int window_max) {
        const double scaled = (static_cast<double>(value) - window_min) * 255.0 / (window_max - window_min);
        return static_cast<int>(std::clamp(std::floor(scaled + 0.5), 0.0, 255.0));
    };

    const std::vector<std::pair<int, int>> windows8 = {{0, 255}, {10, 20}, {0, 1}, {100, 101}, {37, 211}, {0, 254}};
    for (const auto& [window_min, window_max] : windows8) {
        for (int v = 0; v < 256; ++v) {
            ASSERT_EQ(glcm::WindowLevel(v, window_min, window_max), reference(v, window_min, window_max))
                << "v=" << v << " window=[" << window_min << ", " << window_max << "]";
        }
    }

    const std::vector<std::pair<int, int>> windows16 = {{0, 65535}, {0, 4095}, {1000, 3000}, {65534, 65535}, {7, 60001}};
    for (const auto& [window_min, window_max] : windows16) {
        for (int v = 0; v < 65536; v += 97) {
            ASSERT_EQ(glcm::WindowLevel(v, window_min, window_max), reference(v, window_min, window_max))
                << "v=" << v << " window=[" << window_min << ", " << window_max << "]";
        }
    }

    EXPECT_EQ(glcm::WindowLevel(15, 10, 20), 128); // 127.5 rounds up
    EXPECT_EQ(glcm::WindowLevel(9, 10, 20), 0);
    EXPECT_EQ(glcm::WindowLevel(21, 10, 20), 255);
}

TEST(DisplayRendererTest, EmptyWindowIsAThreshold) {
    EXPECT_EQ(glcm::WindowLevel(99, 100, 100), 0);
    EXPECT_EQ(glcm::WindowLevel(100, 100, 100), 255);
    EXPECT_EQ(glcm::WindowLevel(4000, 100, 100), 255);
}

TEST(DisplayRendererTest, RenderAppliesWindowAndDownscales) {
    cv::Mat image(40, 80, CV_16UC1);
    for (int row = 0; row < image.rows; ++row) {
        for (int col = 0; col < image.cols; ++col) {
            image.at<uint16_t>(row, col) = static_cast<uint16_t>(col * 50);
        }
    }

    const cv::Mat full = glcm::RenderWindowLevel(image, 0, 3950);
    ASSERT_EQ(full.type(), CV_8UC1);
    ASSERT_EQ(full.size(), image.size());
    for (int col = 0; col < image.cols; ++col) {
        EXPECT_EQ(full.at<uchar>(7, col), glcm::WindowLevel(col * 50, 0, 3950));
    }

    const cv::Mat small = glcm::RenderWindowLevel(image, 0, 3950, 20);
    EXPECT_EQ(small.size(), cv::Size(20, 10));
    EXPECT_EQ(glcm::RenderWindowLevel(image, 0, 3950, 200).size(), image.size()); // never upscaled
}

TEST(DisplayRendererTest, StatisticsUseNearestRankPercentiles) {
    cv::Mat image(10, 20, CV_8UC1);
    for (int i = 0; i < 200; ++i) {
        image.at<uchar>(i / 20, i % 20) = static_cast<uchar>(i); // 200 distinct values 0 ... 199
    }
    const auto statistics = glcm::ComputeDisplayStatistics(image);
    EXPECT_EQ(statistics.window_min, 0);   // rank ceil(200 * 0.005) = 1
    EXPECT_EQ(statistics.window_max, 198); // rank ceil(200 * 0.995) = 199
    uint64_t total = 0;
    for (uint64_t count : statistics.histogram) {
        total += count;
    }
    EXPECT_EQ(total, 200u);
    EXPECT_EQ(statistics.histogram[5], 1u);
    EXPECT_EQ(statistics.histogram[250], 0u);
}

TEST(DisplayRendererTest, SixteenBitHistogramUses256Bins) {
    cv::Mat image(1, 4, CV_16UC1);
    image.at<uint16_t>(0, 0) = 0;
    image.at<uint16_t>(0, 1) = 255;
    image.at<uint16_t>(0, 2) = 256;
    image.at<uint16_t>(0, 3) = 65535;
    const auto statistics = glcm::ComputeDisplayStatistics(image);
    EXPECT_EQ(statistics.histogram[0], 2u);
    EXPECT_EQ(statistics.histogram[1], 1u);
    EXPECT_EQ(statistics.histogram[255], 1u);

    const cv::Mat constant(5, 5, CV_16UC1, cv::Scalar(777));
    const auto flat = glcm::ComputeDisplayStatistics(constant);
    EXPECT_EQ(flat.window_min, 777);
    EXPECT_EQ(flat.window_max, 777);
}

TEST(DisplayRendererTest, InvalidInputsThrow) {
    const cv::Mat image(4, 4, CV_8UC1, cv::Scalar(1));
    EXPECT_THROW(glcm::RenderWindowLevel(image, 10, 5), std::invalid_argument);
    EXPECT_THROW(glcm::RenderWindowLevel(image, 0, 5, -1), std::invalid_argument);
    EXPECT_THROW(glcm::RenderWindowLevel(cv::Mat(), 0, 5), std::invalid_argument);
    EXPECT_THROW(glcm::ComputeDisplayStatistics(cv::Mat(4, 4, CV_32FC1)), std::invalid_argument);
}

// ---------------------------------------------------------------------------------------------------------------------
// ImageLoader
// ---------------------------------------------------------------------------------------------------------------------

TEST(ImageLoaderTest, Reads16BitTiffWithoutLoss) {
    cv::Mat image(30, 50, CV_16UC1);
    for (int row = 0; row < image.rows; ++row) {
        for (int col = 0; col < image.cols; ++col) {
            image.at<uint16_t>(row, col) = static_cast<uint16_t>((row * 1000 + col * 17) % 65536);
        }
    }
    TemporaryFile file("sixteen.tif");
    ASSERT_TRUE(cv::imwrite(file.path.string(), image));

    const auto loaded = glcm::LoadImageFile(file.path.string());
    EXPECT_TRUE(ImagesEqual(loaded.gray, image));
    EXPECT_EQ(loaded.info.bit_depth, 16);
    EXPECT_EQ(loaded.info.width, 50);
    EXPECT_EQ(loaded.info.height, 30);
    EXPECT_EQ(loaded.info.source_channels, 1);
    EXPECT_TRUE(loaded.warnings.empty());
}

TEST(ImageLoaderTest, ConvertsColorToGrayscale) {
    cv::Mat color(12, 16, CV_8UC3);
    for (int row = 0; row < color.rows; ++row) {
        for (int col = 0; col < color.cols; ++col) {
            color.at<cv::Vec3b>(row, col) = cv::Vec3b(static_cast<uchar>(col * 15), static_cast<uchar>(row * 20), 90);
        }
    }
    TemporaryFile file("color.png");
    ASSERT_TRUE(cv::imwrite(file.path.string(), color));

    cv::Mat expected;
    cv::cvtColor(color, expected, cv::COLOR_BGR2GRAY);
    const auto loaded = glcm::LoadImageFile(file.path.string());
    EXPECT_TRUE(ImagesEqual(loaded.gray, expected));
    EXPECT_EQ(loaded.info.bit_depth, 8);
    EXPECT_EQ(loaded.info.source_channels, 3);
    ASSERT_EQ(loaded.warnings.size(), 1u);
    EXPECT_NE(loaded.warnings[0].find("grayscale"), std::string::npos);
}

TEST(ImageLoaderTest, DecodesImagesFromMemory) {
    cv::Mat image(20, 20, CV_16UC1, cv::Scalar(4242));
    std::vector<uchar> bytes;
    ASSERT_TRUE(cv::imencode(".tif", image, bytes));

    const auto loaded = glcm::LoadImageBytes(bytes);
    EXPECT_TRUE(ImagesEqual(loaded.gray, image));
    EXPECT_EQ(loaded.info.bit_depth, 16);
}

TEST(ImageLoaderTest, RejectsUnreadableAndUnsupportedImages) {
    EXPECT_THROW(glcm::LoadImageFile("/nonexistent/glcm/image.png"), std::runtime_error);
    EXPECT_THROW(glcm::LoadImageBytes({}), std::runtime_error);
    EXPECT_THROW(glcm::LoadImageBytes({'n', 'o', 't', ' ', 'a', 'n', ' ', 'i', 'm', 'a', 'g', 'e'}), std::runtime_error);

    cv::Mat floating(8, 8, CV_32FC1, cv::Scalar(0.5));
    std::vector<uchar> bytes;
    ASSERT_TRUE(cv::imencode(".tif", floating, bytes));
    EXPECT_THROW(glcm::LoadImageBytes(bytes), std::invalid_argument);
}
