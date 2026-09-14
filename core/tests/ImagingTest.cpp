#include <gtest/gtest.h>
#include <unistd.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "imaging/DisplayRenderer.hpp"
#include "imaging/ImageHeader.hpp"
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

TEST(QuantizerTest, TinyBinWidthsAreReportedWithoutOverflow) {
    cv::Mat image(1, 2, CV_16UC1);
    image.at<uint16_t>(0, 0) = 0;
    image.at<uint16_t>(0, 1) = 65535;
    // The number of levels needed does not fit into an integer for tiny widths (and is infinite for the smallest double)
    const std::vector<std::pair<double, std::string>> cases = {
        {1e-15, "needs more than"}, {5e-324, "needs more than"}, {1.0, "needs 65536"}};
    for (const auto& [bin_width, expected] : cases) {
        SCOPED_TRACE(bin_width);
        try {
            Quantize(image, FullMask(image), 256, Settings(QuantizationMethod::FixedBinWidth, 0, 65535, bin_width));
            FAIL() << "expected std::invalid_argument";
        } catch (const std::invalid_argument& error) {
            const std::string message = error.what();
            EXPECT_NE(message.find(expected), std::string::npos) << message;
            EXPECT_EQ(message.find("0.000000"), std::string::npos) << message;
        }
    }
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

// ---------------------------------------------------------------------------------------------------------------------
// ImageHeader and pixel limits
// ---------------------------------------------------------------------------------------------------------------------

namespace {

void Append(std::vector<uchar>& bytes, std::initializer_list<int> values) {
    for (const int value : values) {
        bytes.push_back(static_cast<uchar>(value));
    }
}

// Bytes beyond the eighth are 0 (shifting a 64-bit value by 64 or more bits is undefined)
uchar ByteOf(uint64_t value, int index) {
    return index < 8 ? static_cast<uchar>((value >> (8 * index)) & 0xFF) : 0;
}

void AppendBig(std::vector<uchar>& bytes, uint64_t value, int count) {
    for (int i = count - 1; i >= 0; --i) {
        bytes.push_back(ByteOf(value, i));
    }
}

void AppendLittle(std::vector<uchar>& bytes, uint64_t value, int count) {
    for (int i = 0; i < count; ++i) {
        bytes.push_back(ByteOf(value, i));
    }
}

// Signature and IHDR of a PNG without image data
std::vector<uchar> PngHeader(uint32_t width, uint32_t height) {
    std::vector<uchar> bytes;
    Append(bytes, {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A});
    AppendBig(bytes, 13, 4);
    Append(bytes, {'I', 'H', 'D', 'R'});
    AppendBig(bytes, width, 4);
    AppendBig(bytes, height, 4);
    Append(bytes, {16, 0, 0, 0, 0, 0, 0, 0, 0}); // bit depth, color type, compression, filter, interlace, CRC
    return bytes;
}

std::optional<glcm::ImageSize> SizeOf(const std::vector<uchar>& bytes) {
    return glcm::ReadImageSizeFromBytes(bytes);
}

void ExpectSize(const std::optional<glcm::ImageSize>& size, int64_t width, int64_t height) {
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(size->width, width);
    EXPECT_EQ(size->height, height);
}

} // namespace

TEST(ImageHeaderTest, ReadsTheSizeOfEncodedImages) {
    const cv::Mat gray8(23, 37, CV_8UC1, cv::Scalar(100));
    const cv::Mat gray16(23, 37, CV_16UC1, cv::Scalar(40000));
    const cv::Mat color(23, 37, CV_8UC3, cv::Scalar(10, 20, 30));
    const std::vector<std::pair<std::string, cv::Mat>> cases = {
        {".png", gray8}, {".png", gray16}, {".jpg", color}, {".bmp", gray8}, {".bmp", color}, {".tif", gray8}, {".tif", gray16}};
    for (const auto& [extension, image] : cases) {
        SCOPED_TRACE(extension + " depth " + std::to_string(image.depth()) + " channels " + std::to_string(image.channels()));
        std::vector<uchar> bytes;
        ASSERT_TRUE(cv::imencode(extension, image, bytes));
        ExpectSize(SizeOf(bytes), 37, 23);

        TemporaryFile file("header" + extension);
        ASSERT_TRUE(cv::imwrite(file.path.string(), image));
        ExpectSize(glcm::ReadImageSize(file.path.string()), 37, 23);
    }
}

TEST(ImageHeaderTest, ReadsDeclaredSizesWithoutImageData) {
    ExpectSize(SizeOf(PngHeader(30000, 20000)), 30000, 20000);
    ExpectSize(SizeOf(PngHeader(0xFFFFFFFF, 0xFFFFFFFF)), 0xFFFFFFFFLL, 0xFFFFFFFFLL);

    // JPEG: fill bytes, an APP1 segment containing a thumbnail frame header that must be skipped, then SOF2
    std::vector<uchar> jpeg;
    Append(jpeg, {0xFF, 0xD8, 0xFF, 0xFF, 0xE1});
    AppendBig(jpeg, 2 + 9, 2);
    Append(jpeg, {0xFF, 0xC0, 0x00, 0x11, 0x08, 0x00, 0x10, 0x00, 0x10});
    Append(jpeg, {0xFF, 0xC2});
    AppendBig(jpeg, 17, 2);
    Append(jpeg, {8});
    AppendBig(jpeg, 1000, 2);  // height
    AppendBig(jpeg, 65535, 2); // width
    ExpectSize(SizeOf(jpeg), 65535, 1000);

    // BMP with BITMAPINFOHEADER and a negative (top-down) height, and with the OS/2 core header
    std::vector<uchar> bmp;
    Append(bmp, {'B', 'M'});
    AppendLittle(bmp, 0, 12);
    AppendLittle(bmp, 40, 4);
    AppendLittle(bmp, 50000, 4);
    AppendLittle(bmp, static_cast<uint32_t>(-40000), 4);
    ExpectSize(SizeOf(bmp), 50000, 40000);
    std::vector<uchar> core_bmp;
    Append(core_bmp, {'B', 'M'});
    AppendLittle(core_bmp, 0, 12);
    AppendLittle(core_bmp, 12, 4);
    AppendLittle(core_bmp, 640, 2);
    AppendLittle(core_bmp, 480, 2);
    ExpectSize(SizeOf(core_bmp), 640, 480);

    // Big-endian TIFF: an unrelated tag, ImageWidth as SHORT and ImageLength as LONG
    std::vector<uchar> tiff;
    Append(tiff, {'M', 'M', 0, 42});
    AppendBig(tiff, 8, 4);
    AppendBig(tiff, 3, 2);
    for (const auto& [tag, type, value] : std::vector<std::tuple<int, int, uint32_t>>{{254, 4, 0}, {256, 3, 40000}, {257, 4, 70000}}) {
        AppendBig(tiff, tag, 2);
        AppendBig(tiff, type, 2);
        AppendBig(tiff, 1, 4);
        type == 3 ? (AppendBig(tiff, value, 2), AppendBig(tiff, 0, 2)) : AppendBig(tiff, value, 4);
    }
    ExpectSize(SizeOf(tiff), 40000, 70000);

    // Little-endian BigTIFF with a LONG8 width above 32 bits
    std::vector<uchar> bigtiff;
    Append(bigtiff, {'I', 'I'});
    AppendLittle(bigtiff, 43, 2);
    AppendLittle(bigtiff, 8, 2);
    AppendLittle(bigtiff, 0, 2);
    AppendLittle(bigtiff, 16, 8);
    AppendLittle(bigtiff, 2, 8);
    for (const auto& [tag, type, value] : std::vector<std::tuple<int, int, uint64_t>>{{256, 16, 5000000000ULL}, {257, 3, 3}}) {
        AppendLittle(bigtiff, tag, 2);
        AppendLittle(bigtiff, type, 2);
        AppendLittle(bigtiff, 1, 8);
        AppendLittle(bigtiff, value, 8);
    }
    ExpectSize(SizeOf(bigtiff), 5000000000LL, 3);
}

TEST(ImageHeaderTest, RejectsUnknownTruncatedAndInvalidHeaders) {
    EXPECT_FALSE(SizeOf({'G', 'I', 'F', '8', '9', 'a', 1, 0, 1, 0}).has_value());
    EXPECT_FALSE(SizeOf({}).has_value());
    EXPECT_THROW(glcm::ReadImageSize("/nonexistent/glcm/image.png"), std::runtime_error);

    std::vector<uchar> truncated = PngHeader(10, 10);
    truncated.resize(20);
    EXPECT_THROW(SizeOf(truncated), std::runtime_error);
    EXPECT_THROW(SizeOf(PngHeader(0, 10)), std::runtime_error);
    EXPECT_THROW(SizeOf({0xFF, 0xD8, 0xFF, 0xDA, 0x00, 0x02}), std::runtime_error); // image data before a frame header
    EXPECT_THROW(SizeOf({'I', 'I', 42, 0, 8, 0, 0, 0, 0, 0}), std::runtime_error);  // directory without a size
    EXPECT_THROW(SizeOf({'M', 'M', 0, 41, 0, 0, 0, 8}), std::runtime_error);        // unknown TIFF version
}

TEST(ImageLoaderTest, ChecksThePixelLimitBeforeDecoding) {
    // A decoder would need gigabytes for this header; the limit rejects it without decoding (the file has no image data)
    const std::vector<uchar> bomb = PngHeader(30000, 30000);
    TemporaryFile file("bomb.png");
    {
        std::ofstream stream(file.path, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(bomb.data()), static_cast<std::streamsize>(bomb.size()));
    }
    EXPECT_THROW(glcm::LoadImageFile(file.path.string(), 100000000), glcm::ImageTooLargeError);
    EXPECT_THROW(glcm::LoadImageBytes(bomb, 100000000), glcm::ImageTooLargeError);
    try {
        glcm::LoadImageBytes(PngHeader(0xFFFFFFFF, 0xFFFFFFFF), 1);
        FAIL() << "expected ImageTooLargeError";
    } catch (const glcm::ImageTooLargeError& error) {
        EXPECT_NE(std::string(error.what()).find("4294967295 x 4294967295"), std::string::npos) << error.what();
    }

    // Without a limit, the same file reaches the decoder, which cannot read it
    EXPECT_THROW(glcm::LoadImageFile(file.path.string()), std::runtime_error);
}

TEST(ImageLoaderTest, AppliesThePixelLimitExactly) {
    const cv::Mat image(30, 50, CV_16UC1, cv::Scalar(1234));
    TemporaryFile file("limit.tif");
    ASSERT_TRUE(cv::imwrite(file.path.string(), image));
    std::vector<uchar> bytes;
    ASSERT_TRUE(cv::imencode(".png", image, bytes));

    EXPECT_EQ(glcm::LoadImageFile(file.path.string(), 1500).info.width, 50);
    EXPECT_EQ(glcm::LoadImageBytes(bytes, 1500).info.height, 30);
    EXPECT_THROW(glcm::LoadImageFile(file.path.string(), 1499), glcm::ImageTooLargeError);
    EXPECT_THROW(glcm::LoadImageBytes(bytes, 1499), glcm::ImageTooLargeError);

    // Formats whose size cannot be read in advance are refused only when a limit is set
    std::vector<uchar> pgm;
    ASSERT_TRUE(cv::imencode(".pgm", cv::Mat(4, 4, CV_8UC1, cv::Scalar(7)), pgm));
    EXPECT_EQ(glcm::LoadImageBytes(pgm).info.width, 4);
    try {
        glcm::LoadImageBytes(pgm, 1000);
        FAIL() << "expected std::runtime_error";
    } catch (const glcm::ImageTooLargeError&) {
        FAIL() << "unknown formats are not reported as too large";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("Unknown image format"), std::string::npos) << error.what();
    }
}
