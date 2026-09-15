// Node-API bindings for glcm_core (doc/ui-design-plan.md, phases 1 and 3).
//
// Long-running work (decoding, rendering, ROI statistics, analyses) runs in Napi::AsyncWorker threads and returns
// promises. Rejected promises and errors thrown by synchronous validation carry an error `code`: INVALID_ARGUMENT,
// UNSUPPORTED_IMAGE, IMAGE_TOO_LARGE, DECODE_FAILED, INTERNAL_ERROR or CANCELLED (a cancelled feature map).

#include <napi.h>

#include <atomic>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <opencv2/imgcodecs.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "imaging/DisplayRenderer.hpp"
#include "imaging/ImageLoader.hpp"
#include "io/Identifiers.hpp"
#include "io/Json.hpp"
#include "io/ResultsCsv.hpp"
#include "io/RoiImageExport.hpp"
#include "pipeline/AnalysisRunner.hpp"
#include "pipeline/AnalysisSettings.hpp"
#include "pipeline/FeatureCatalog.hpp"
#include "pipeline/FeatureMap.hpp"
#include "pipeline/Version.hpp"
#include "roi/Roi.hpp"

namespace {

const char CODE_INVALID_ARGUMENT[] = "INVALID_ARGUMENT";
const char CODE_UNSUPPORTED_IMAGE[] = "UNSUPPORTED_IMAGE";
const char CODE_DECODE_FAILED[] = "DECODE_FAILED";
const char CODE_IMAGE_TOO_LARGE[] = "IMAGE_TOO_LARGE";
const char CODE_INTERNAL[] = "INTERNAL_ERROR";
const char CODE_CANCELLED[] = "CANCELLED";

bool HostIsLittleEndian() {
    const uint16_t probe = 1;
    return *reinterpret_cast<const uint8_t*>(&probe) == 1;
}

// The rows of a CV_8UC1 or CV_16UC1 image as bytes, with 16-bit samples in little-endian order
std::vector<uint8_t> ToLittleEndianBytes(const cv::Mat& gray) {
    const size_t bytes_per_sample = gray.elemSize();
    const size_t line_bytes = static_cast<size_t>(gray.cols) * bytes_per_sample;
    std::vector<uint8_t> bytes(line_bytes * static_cast<size_t>(gray.rows));
    for (int row = 0; row < gray.rows; ++row) {
        const uint8_t* source = gray.ptr<uint8_t>(row);
        uint8_t* target = bytes.data() + line_bytes * static_cast<size_t>(row);
        if (bytes_per_sample == 1 || HostIsLittleEndian()) {
            std::memcpy(target, source, line_bytes);
        } else {
            for (size_t i = 0; i < line_bytes; i += 2) {
                target[i] = source[i + 1];
                target[i + 1] = source[i];
            }
        }
    }
    return bytes;
}

// CV_8UC1 or CV_16UC1 image from row-major little-endian bytes. The bytes are wrapped without copying when they can be
// read in place (8-bit, or 16-bit on a little-endian host with aligned data); the caller must keep them alive.
cv::Mat FromLittleEndianBytes(const uint8_t* data, int width, int height, int bit_depth) {
    const int type = bit_depth == 16 ? CV_16UC1 : CV_8UC1;
    const bool aligned = reinterpret_cast<uintptr_t>(data) % alignof(uint16_t) == 0;
    if (bit_depth == 8 || (HostIsLittleEndian() && aligned)) {
        // cv::Mat has no read-only header; the workers only read from it
        return cv::Mat(height, width, type, const_cast<void*>(static_cast<const void*>(data)));
    }

    cv::Mat gray(height, width, type);
    const size_t line_bytes = static_cast<size_t>(width) * 2;
    for (int row = 0; row < height; ++row) {
        const uint8_t* source = data + line_bytes * static_cast<size_t>(row);
        uint8_t* target = gray.ptr<uint8_t>(row);
        if (HostIsLittleEndian()) {
            std::memcpy(target, source, line_bytes);
        } else {
            for (size_t i = 0; i < line_bytes; i += 2) {
                target[i] = source[i + 1];
                target[i + 1] = source[i];
            }
        }
    }
    return gray;
}

int IntegerArgument(const Napi::CallbackInfo& info, size_t index, const char* name) {
    if (info.Length() <= index || !info[index].IsNumber()) {
        throw Napi::TypeError::New(info.Env(), std::string(name) + " must be a number");
    }
    const double value = info[index].As<Napi::Number>().DoubleValue();
    if (!std::isfinite(value) || std::floor(value) != value || value < INT_MIN || value > INT_MAX) {
        throw Napi::TypeError::New(info.Env(), std::string(name) + " must be an integer");
    }
    return static_cast<int>(value);
}

std::string StringArgument(const Napi::CallbackInfo& info, size_t index, const char* name) {
    if (info.Length() <= index || !info[index].IsString()) {
        throw Napi::TypeError::New(info.Env(), std::string(name) + " must be a string");
    }
    return info[index].As<Napi::String>().Utf8Value();
}

bool BooleanArgument(const Napi::CallbackInfo& info, size_t index, const char* name) {
    if (info.Length() <= index || !info[index].IsBoolean()) {
        throw Napi::TypeError::New(info.Env(), std::string(name) + " must be a boolean");
    }
    return info[index].As<Napi::Boolean>().Value();
}

Napi::Array StringArray(Napi::Env env, const std::vector<std::string>& values) {
    Napi::Array array = Napi::Array::New(env, values.size());
    for (size_t i = 0; i < values.size(); ++i) {
        array.Set(static_cast<uint32_t>(i), Napi::String::New(env, values[i]));
    }
    return array;
}

Napi::Value NumberOrNull(Napi::Env env, double value) {
    return std::isfinite(value) ? Napi::Number::New(env, value) : env.Null();
}

Napi::Error ErrorWithCode(Napi::Env env, const char* code, const std::string& message) {
    Napi::Error error = Napi::Error::New(env, message);
    error.Set("code", Napi::String::New(env, code));
    return error;
}

// Grayscale pixels passed from JavaScript as (pixels, width, height, bitDepth), starting at argument `first`
struct PixelArguments {
    Napi::Uint8Array pixels;
    int width = 0;
    int height = 0;
    int bit_depth = 0;
};

PixelArguments ReadPixelArguments(const Napi::CallbackInfo& info, size_t first) {
    Napi::Env env = info.Env();
    if (info.Length() <= first || !info[first].IsTypedArray() || info[first].As<Napi::TypedArray>().TypedArrayType() != napi_uint8_array) {
        throw Napi::TypeError::New(env, "pixels must be a Uint8Array or Buffer");
    }
    PixelArguments arguments;
    arguments.pixels = info[first].As<Napi::Uint8Array>();
    arguments.width = IntegerArgument(info, first + 1, "width");
    arguments.height = IntegerArgument(info, first + 2, "height");
    arguments.bit_depth = IntegerArgument(info, first + 3, "bitDepth");

    if (arguments.width <= 0 || arguments.height <= 0) {
        throw Napi::TypeError::New(env, "width and height must be positive");
    }
    if (arguments.bit_depth != 8 && arguments.bit_depth != 16) {
        throw Napi::TypeError::New(env, "bitDepth must be 8 or 16");
    }
    const size_t expected =
        static_cast<size_t>(arguments.width) * static_cast<size_t>(arguments.height) * static_cast<size_t>(arguments.bit_depth / 8);
    if (arguments.pixels.ByteLength() != expected) {
        throw Napi::TypeError::New(
            env, "pixels has " + std::to_string(arguments.pixels.ByteLength()) + " bytes, expected " + std::to_string(expected));
    }
    return arguments;
}

// ROI objects as used in the ROI set file format, parsed from a JSON array
std::vector<glcm::Roi> ParseRois(const std::string& rois_json) {
    return glcm::RoiSetFromJson(R"({"format": "glcm-roi-set", "version": 1, "rois": )" + rois_json + "}").rois;
}

// AsyncWorker that settles a promise and rejects with an Error carrying `code`
class PromiseWorker : public Napi::AsyncWorker {
public:
    explicit PromiseWorker(Napi::Env env) : Napi::AsyncWorker(env), _deferred(Napi::Promise::Deferred::New(env)) {}

    Napi::Promise Promise() const {
        return _deferred.Promise();
    }

protected:
    void Fail(const char* code, const std::string& message) {
        _code = code;
        SetError(message);
    }

    void Resolve(Napi::Value value) {
        _deferred.Resolve(value);
    }

    void OnError(const Napi::Error& error) override {
        _deferred.Reject(ErrorWithCode(Env(), _code.empty() ? CODE_INTERNAL : _code.c_str(), error.Message()).Value());
    }

private:
    Napi::Promise::Deferred _deferred;
    std::string _code;
};

// Worker reading a JavaScript pixel buffer, which it keeps alive until it finishes
class PixelWorker : public PromiseWorker {
public:
    PixelWorker(Napi::Env env, const PixelArguments& arguments)
        : PromiseWorker(env),
          _pixels(Napi::Persistent(arguments.pixels)),
          _data(arguments.pixels.Data()),
          _width(arguments.width),
          _height(arguments.height),
          _bit_depth(arguments.bit_depth) {}

protected:
    cv::Mat Gray() const {
        return FromLittleEndianBytes(_data, _width, _height, _bit_depth);
    }

private:
    Napi::Reference<Napi::Uint8Array> _pixels;
    const uint8_t* _data;
    int _width;
    int _height;
    int _bit_depth;
};

class DecodeImageWorker : public PromiseWorker {
public:
    DecodeImageWorker(Napi::Env env, std::string path, int64_t max_pixels)
        : PromiseWorker(env), _path(std::move(path)), _max_pixels(max_pixels) {}

    void Execute() override {
        try {
            glcm::LoadedImage image = glcm::LoadImageFile(_path, _max_pixels);
            _info = image.info;
            _warnings = image.warnings;
            _statistics = glcm::ComputeDisplayStatistics(image.gray);
            _pixels = ToLittleEndianBytes(image.gray);
        } catch (const glcm::ImageTooLargeError& error) {
            Fail(CODE_IMAGE_TOO_LARGE, error.what());
        } catch (const std::invalid_argument& error) {
            Fail(CODE_UNSUPPORTED_IMAGE, error.what());
        } catch (const std::exception& error) {
            Fail(CODE_DECODE_FAILED, error.what());
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        Napi::Object result = Napi::Object::New(env);
        result.Set("width", Napi::Number::New(env, _info.width));
        result.Set("height", Napi::Number::New(env, _info.height));
        result.Set("bitDepth", Napi::Number::New(env, _info.bit_depth));
        result.Set("sourceChannels", Napi::Number::New(env, _info.source_channels));
        if (_info.pixel_spacing) {
            Napi::Object spacing = Napi::Object::New(env);
            spacing.Set("x", Napi::Number::New(env, _info.pixel_spacing->x_mm));
            spacing.Set("y", Napi::Number::New(env, _info.pixel_spacing->y_mm));
            result.Set("pixelSpacing", spacing);
        } else {
            result.Set("pixelSpacing", env.Null());
        }
        result.Set("warnings", StringArray(env, _warnings));
        result.Set("windowMin", Napi::Number::New(env, _statistics.window_min));
        result.Set("windowMax", Napi::Number::New(env, _statistics.window_max));

        Napi::Array histogram = Napi::Array::New(env, _statistics.histogram.size());
        for (size_t i = 0; i < _statistics.histogram.size(); ++i) {
            histogram.Set(static_cast<uint32_t>(i), Napi::Number::New(env, static_cast<double>(_statistics.histogram[i])));
        }
        result.Set("histogram", histogram);
        result.Set("pixels", Napi::Buffer<uint8_t>::Copy(env, _pixels.data(), _pixels.size()));
        Resolve(result);
    }

private:
    std::string _path;
    int64_t _max_pixels;
    glcm::ImageInfo _info;
    std::vector<std::string> _warnings;
    glcm::DisplayStatistics _statistics;
    std::vector<uint8_t> _pixels;
};

class RenderDisplayWorker : public PixelWorker {
public:
    RenderDisplayWorker(Napi::Env env, const PixelArguments& arguments, int window_min, int window_max, int max_size)
        : PixelWorker(env, arguments), _window_min(window_min), _window_max(window_max), _max_size(max_size) {}

    void Execute() override {
        try {
            const cv::Mat rendered = glcm::RenderWindowLevel(Gray(), _window_min, _window_max, _max_size);
            if (!cv::imencode(".png", rendered, _png)) {
                Fail(CODE_INTERNAL, "Cannot encode the PNG image");
            }
        } catch (const std::invalid_argument& error) {
            Fail(CODE_INVALID_ARGUMENT, error.what());
        } catch (const std::exception& error) {
            Fail(CODE_INTERNAL, error.what());
        }
    }

    void OnOK() override {
        Resolve(Napi::Buffer<uint8_t>::Copy(Env(), _png.data(), _png.size()));
    }

private:
    int _window_min;
    int _window_max;
    int _max_size;
    std::vector<uchar> _png;
};

struct RoiStatisticsResult {
    std::string error; // non-empty when the geometry is invalid
    glcm::RegionStatistics statistics;
    cv::Rect bounding_box;
};

class RoiStatsWorker : public PixelWorker {
public:
    RoiStatsWorker(Napi::Env env, const PixelArguments& arguments, std::string rois_json)
        : PixelWorker(env, arguments), _rois_json(std::move(rois_json)) {}

    void Execute() override {
        std::vector<glcm::Roi> rois;
        try {
            rois = ParseRois(_rois_json);
        } catch (const std::invalid_argument& error) {
            Fail(CODE_INVALID_ARGUMENT, error.what());
            return;
        }
        try {
            const cv::Mat gray = Gray();
            for (const glcm::Roi& roi : rois) {
                RoiStatisticsResult result;
                try {
                    // Only the box around the ROI is rasterized and visited
                    const glcm::CroppedMask cropped = glcm::RasterizeCroppedMask(roi.shape, gray.size());
                    if (!cropped.mask.empty()) {
                        result.statistics = glcm::ComputeRegionStatistics(gray(cropped.box), cropped.mask);
                        const cv::Rect box = glcm::MaskBoundingBox(cropped.mask);
                        result.bounding_box = box.area() > 0 ? box + cropped.box.tl() : cv::Rect();
                    }
                } catch (const std::invalid_argument& error) {
                    result.error = error.what();
                }
                _results.push_back(result);
            }
        } catch (const std::exception& error) {
            Fail(CODE_INTERNAL, error.what());
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        Napi::Array array = Napi::Array::New(env, _results.size());
        for (size_t i = 0; i < _results.size(); ++i) {
            const RoiStatisticsResult& result = _results[i];
            const glcm::RegionStatistics& statistics = result.statistics;
            const bool empty = statistics.pixel_count == 0;

            Napi::Object item = Napi::Object::New(env);
            item.Set("pixelCount", Napi::Number::New(env, statistics.pixel_count));
            if (empty) {
                item.Set("boundingBox", env.Null());
            } else {
                Napi::Object box = Napi::Object::New(env);
                box.Set("x", result.bounding_box.x);
                box.Set("y", result.bounding_box.y);
                box.Set("width", result.bounding_box.width);
                box.Set("height", result.bounding_box.height);
                item.Set("boundingBox", box);
            }
            item.Set("min", empty ? env.Null() : Napi::Number::New(env, statistics.min));
            item.Set("max", empty ? env.Null() : Napi::Number::New(env, statistics.max));
            item.Set("mean", NumberOrNull(env, statistics.mean));
            item.Set("std", NumberOrNull(env, statistics.std));
            item.Set("error", result.error.empty() ? env.Null() : Napi::String::New(env, result.error));
            array.Set(static_cast<uint32_t>(i), item);
        }
        Resolve(array);
    }

private:
    std::string _rois_json;
    std::vector<RoiStatisticsResult> _results;
};

class RunAnalysisWorker : public PixelWorker {
public:
    RunAnalysisWorker(Napi::Env env, const PixelArguments& arguments, std::string rois_json, std::string settings_json)
        : PixelWorker(env, arguments), _rois_json(std::move(rois_json)), _settings_json(std::move(settings_json)) {}

    void Execute() override {
        glcm::AnalysisSettings settings;
        std::vector<glcm::Roi> rois;
        try {
            rois = ParseRois(_rois_json);
            settings = glcm::SettingsFromJson(_settings_json);
            glcm::ValidateSettings(settings);
        } catch (const std::invalid_argument& error) {
            Fail(CODE_INVALID_ARGUMENT, error.what());
            return;
        }
        try {
            const glcm::AnalysisOutput output = glcm::RunAnalysis(Gray(), rois, settings);
            _json = glcm::ResultsToJson(output.results, settings, glcm::ExportContext{});
        } catch (const std::invalid_argument& error) {
            Fail(CODE_INVALID_ARGUMENT, error.what());
        } catch (const std::exception& error) {
            Fail(CODE_INTERNAL, error.what());
        }
    }

    void OnOK() override {
        Resolve(Napi::String::New(Env(), _json));
    }

private:
    std::string _rois_json;
    std::string _settings_json;
    std::string _json;
};

class ExportRoiImagesWorker : public PixelWorker {
public:
    ExportRoiImagesWorker(Napi::Env env, const PixelArguments& arguments, std::string rois_json, std::string settings_json,
        const glcm::RoiImageExportOptions& options)
        : PixelWorker(env, arguments), _rois_json(std::move(rois_json)), _settings_json(std::move(settings_json)), _options(options) {}

    void Execute() override {
        std::vector<glcm::Roi> rois;
        glcm::AnalysisSettings settings;
        try {
            rois = ParseRois(_rois_json);
            if (!_settings_json.empty()) {
                settings = glcm::SettingsFromJson(_settings_json);
                glcm::ValidateSettings(settings);
            } else if (_options.include_quantized) {
                throw std::invalid_argument("Quantized ROI images need analysis settings");
            }
        } catch (const std::invalid_argument& error) {
            Fail(CODE_INVALID_ARGUMENT, error.what());
            return;
        }
        try {
            _files = glcm::ExportRoiImages(Gray(), rois, settings, _options);
        } catch (const std::invalid_argument& error) {
            Fail(CODE_INVALID_ARGUMENT, error.what());
        } catch (const std::exception& error) {
            Fail(CODE_INTERNAL, error.what());
        }
    }

    void OnOK() override {
        Napi::Env env = Env();
        Napi::Array array = Napi::Array::New(env, _files.size());
        for (size_t i = 0; i < _files.size(); ++i) {
            Napi::Object file = Napi::Object::New(env);
            file.Set("name", _files[i].name);
            file.Set("data", Napi::Buffer<uint8_t>::Copy(env, _files[i].bytes.data(), _files[i].bytes.size()));
            array.Set(static_cast<uint32_t>(i), file);
        }
        Resolve(array);
    }

private:
    std::string _rois_json;
    std::string _settings_json;
    glcm::RoiImageExportOptions _options;
    std::vector<glcm::ExportedFile> _files;
};

Napi::Value CoreVersion(const Napi::CallbackInfo& info) {
    return Napi::String::New(info.Env(), glcm::CORE_VERSION);
}

Napi::Value Catalog(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    const auto& catalog = glcm::FeatureCatalog();

    Napi::Array features = Napi::Array::New(env, catalog.size());
    for (size_t i = 0; i < catalog.size(); ++i) {
        const glcm::FeatureInfo& feature = catalog[i];
        Napi::Object item = Napi::Object::New(env);
        item.Set("id", feature.id);
        item.Set("name", feature.name);
        item.Set("group", glcm::FeatureGroupId(feature.group));
        item.Set("nonStandard", feature.non_standard);
        item.Set("nonStandardReason", feature.non_standard_reason);
        item.Set("docAnchor", feature.doc_anchor);
        item.Set("cost", feature.cost == glcm::FeatureCost::Slow ? "slow" : "normal");
        features.Set(static_cast<uint32_t>(i), item);
    }

    const auto& presets = glcm::FeaturePresets();
    Napi::Array preset_array = Napi::Array::New(env, presets.size());
    for (size_t i = 0; i < presets.size(); ++i) {
        const glcm::FeaturePreset& preset = presets[i];
        std::vector<std::string> ids;
        for (const glcm::FeatureInfo& feature : catalog) {
            if (preset.features.count(feature.type) > 0) {
                ids.push_back(feature.id);
            }
        }
        Napi::Object item = Napi::Object::New(env);
        item.Set("id", preset.id);
        item.Set("name", preset.name);
        item.Set("features", StringArray(env, ids));
        item.Set("enablesScore", preset.enables_score);
        preset_array.Set(static_cast<uint32_t>(i), item);
    }

    Napi::Array directions = Napi::Array::New(env, glcm::DIRECTIONS_BY_ANGLE.size());
    for (size_t i = 0; i < glcm::DIRECTIONS_BY_ANGLE.size(); ++i) {
        directions.Set(static_cast<uint32_t>(i), Napi::Number::New(env, glcm::DirectionAngle(glcm::DIRECTIONS_BY_ANGLE[i])));
    }

    const glcm::ScoreCoefficients coefficients;
    Napi::Object default_coefficients = Napi::Object::New(env);
    default_coefficients.Set("age", coefficients.age);
    default_coefficients.Set("mean", coefficients.mean);
    default_coefficients.Set("entropy", coefficients.entropy);
    default_coefficients.Set("contrast", coefficients.contrast);

    Napi::Object limits = Napi::Object::New(env);
    limits.Set("minGrayLevels", Napi::Number::New(env, glcm::MIN_GRAY_LEVELS));
    limits.Set("maxGrayLevels", Napi::Number::New(env, glcm::MAX_GRAY_LEVELS));
    limits.Set("defaultGrayLevels", Napi::Number::New(env, glcm::AnalysisSettings().gray_levels));
    limits.Set("maxDistance", Napi::Number::New(env, glcm::MAX_DISTANCE));
    limits.Set("directions", directions);
    limits.Set("quantizationMethods", StringArray(env, {glcm::QuantizationMethodId(glcm::QuantizationMethod::FixedRange),
                                                           glcm::QuantizationMethodId(glcm::QuantizationMethod::RoiMinMax),
                                                           glcm::QuantizationMethodId(glcm::QuantizationMethod::FixedBinWidth),
                                                           glcm::QuantizationMethodId(glcm::QuantizationMethod::None)}));
    limits.Set("aggregations",
        StringArray(env, {glcm::AggregationId(glcm::Aggregation::PerDirectionAndMean), glcm::AggregationId(glcm::Aggregation::MeanOnly),
                             glcm::AggregationId(glcm::Aggregation::MeanAndRange)}));
    limits.Set("logBases", StringArray(env, {glcm::LogBaseId(glcm::LogBase::Natural), glcm::LogBaseId(glcm::LogBase::Two)}));
    limits.Set("scoreProfiles", StringArray(env, {glcm::ScoreProfileId(glcm::ScoreProfile::Calibration),
                                                     glcm::ScoreProfileId(glcm::ScoreProfile::CurrentSettings)}));
    limits.Set("defaultScoreCoefficients", default_coefficients);

    Napi::Object result = Napi::Object::New(env);
    result.Set("features", features);
    result.Set("presets", preset_array);
    result.Set("limits", limits);
    return result;
}

// maxPixels of an optional options object at `index`; 0 (no limit) when absent
int64_t MaxPixelsOption(const Napi::CallbackInfo& info, size_t index) {
    if (info.Length() <= index || info[index].IsUndefined()) {
        return 0;
    }
    if (!info[index].IsObject()) {
        throw Napi::TypeError::New(info.Env(), "options must be an object");
    }
    const Napi::Value value = info[index].As<Napi::Object>().Get("maxPixels");
    if (value.IsUndefined()) {
        return 0;
    }
    const double number = value.IsNumber() ? value.As<Napi::Number>().DoubleValue() : -1.0;
    if (!std::isfinite(number) || std::floor(number) != number || number < 0 || number > 9007199254740991.0) {
        throw Napi::TypeError::New(info.Env(), "options.maxPixels must be a non-negative integer");
    }
    return static_cast<int64_t>(number);
}

// decodeImageFile(path: string, options?: {maxPixels?: number}): Promise<DecodedImage>
Napi::Value DecodeImageFile(const Napi::CallbackInfo& info) {
    auto* worker = new DecodeImageWorker(info.Env(), StringArgument(info, 0, "path"), MaxPixelsOption(info, 1));
    const Napi::Promise promise = worker->Promise();
    worker->Queue();
    return promise;
}

// renderDisplay(pixels, width, height, bitDepth, windowMin, windowMax, maxSize): Promise<Buffer> (PNG)
Napi::Value RenderDisplay(const Napi::CallbackInfo& info) {
    const PixelArguments pixels = ReadPixelArguments(info, 0);
    const int window_min = IntegerArgument(info, 4, "windowMin");
    const int window_max = IntegerArgument(info, 5, "windowMax");
    const int max_size = IntegerArgument(info, 6, "maxSize");

    auto* worker = new RenderDisplayWorker(info.Env(), pixels, window_min, window_max, max_size);
    const Napi::Promise promise = worker->Promise();
    worker->Queue();
    return promise;
}

// roiStats(pixels, width, height, bitDepth, roisJson): Promise<RoiStatistics[]>
Napi::Value RoiStats(const Napi::CallbackInfo& info) {
    const PixelArguments pixels = ReadPixelArguments(info, 0);
    auto* worker = new RoiStatsWorker(info.Env(), pixels, StringArgument(info, 4, "roisJson"));
    const Napi::Promise promise = worker->Promise();
    worker->Queue();
    return promise;
}

// validateAnalysis(roisJson, settingsJson): throws an Error with code INVALID_ARGUMENT when either is invalid
Napi::Value ValidateAnalysis(const Napi::CallbackInfo& info) {
    const std::string rois_json = StringArgument(info, 0, "roisJson");
    const std::string settings_json = StringArgument(info, 1, "settingsJson");
    try {
        ParseRois(rois_json);
        glcm::ValidateSettings(glcm::SettingsFromJson(settings_json));
    } catch (const std::invalid_argument& error) {
        throw ErrorWithCode(info.Env(), CODE_INVALID_ARGUMENT, error.what());
    } catch (const std::exception& error) {
        // Without this, e.g. std::bad_alloc would cross the Node-API boundary and terminate the process
        throw ErrorWithCode(info.Env(), CODE_INTERNAL, error.what());
    }
    return info.Env().Undefined();
}

// runAnalysis(pixels, width, height, bitDepth, roisJson, settingsJson): Promise<string> (glcm-results JSON)
Napi::Value RunAnalysis(const Napi::CallbackInfo& info) {
    const PixelArguments pixels = ReadPixelArguments(info, 0);
    auto* worker = new RunAnalysisWorker(info.Env(), pixels, StringArgument(info, 4, "roisJson"), StringArgument(info, 5, "settingsJson"));
    const Napi::Promise promise = worker->Promise();
    worker->Queue();
    return promise;
}

// formatResults(resultsJson, format): string; a "glcm-results" document written again by glcm_core as "csv" or "json"
Napi::Value FormatResults(const Napi::CallbackInfo& info) {
    const std::string text = StringArgument(info, 0, "resultsJson");
    const std::string format = StringArgument(info, 1, "format");
    if (format != "csv" && format != "json") {
        throw Napi::TypeError::New(info.Env(), "format must be \"csv\" or \"json\"");
    }
    try {
        const glcm::ResultsDocument document = glcm::ResultsFromJson(text);
        const std::string output = format == "csv" ? glcm::ResultsToCsv(document.results, document.settings, document.context)
                                                   : glcm::ResultsToJson(document.results, document.settings, document.context);
        return Napi::String::New(info.Env(), output);
    } catch (const std::invalid_argument& error) {
        throw ErrorWithCode(info.Env(), CODE_INVALID_ARGUMENT, error.what());
    } catch (const std::exception& error) {
        // Without this, e.g. std::bad_alloc for a huge document would cross the Node-API boundary and terminate the process
        throw ErrorWithCode(info.Env(), CODE_INTERNAL, error.what());
    }
}

// exportRoiImages(pixels, width, height, bitDepth, roisJson, settingsJson, transparentOutside, includeQuantized):
// Promise<{name, data}[]>; settingsJson may be empty unless includeQuantized
Napi::Value ExportRoiImages(const Napi::CallbackInfo& info) {
    const PixelArguments pixels = ReadPixelArguments(info, 0);
    glcm::RoiImageExportOptions options;
    options.transparent_outside = BooleanArgument(info, 6, "transparentOutside");
    options.include_quantized = BooleanArgument(info, 7, "includeQuantized");
    auto* worker = new ExportRoiImagesWorker(
        info.Env(), pixels, StringArgument(info, 4, "roisJson"), StringArgument(info, 5, "settingsJson"), options);
    const Napi::Promise promise = worker->Promise();
    worker->Queue();
    return promise;
}

// windowLevel(value, windowMin, windowMax): number
Napi::Value WindowLevel(const Napi::CallbackInfo& info) {
    const int value = IntegerArgument(info, 0, "value");
    const int window_min = IntegerArgument(info, 1, "windowMin");
    const int window_max = IntegerArgument(info, 2, "windowMax");
    return Napi::Number::New(info.Env(), glcm::WindowLevel(value, window_min, window_max));
}

// Marks CancelToken objects, so that computeFeatureMap never unwraps another kind of object
const napi_type_tag CANCEL_TOKEN_TAG = {0x8a4f2c1e5b7d4e3aULL, 0x9c6b1f0d2e8a7c55ULL};

// new CancelToken(): cancel() makes the computeFeatureMap calls that received the token stop after their current point
class CancelToken : public Napi::ObjectWrap<CancelToken> {
public:
    static Napi::Function Define(Napi::Env env) {
        return DefineClass(env, "CancelToken",
            {InstanceMethod("cancel", &CancelToken::Cancel), InstanceAccessor("cancelled", &CancelToken::Cancelled, nullptr)});
    }

    explicit CancelToken(const Napi::CallbackInfo& info)
        : Napi::ObjectWrap<CancelToken>(info), _flag(std::make_shared<std::atomic<bool>>(false)) {
        info.This().As<Napi::Object>().TypeTag(&CANCEL_TOKEN_TAG);
    }

    std::shared_ptr<std::atomic<bool>> Flag() const {
        return _flag;
    }

private:
    void Cancel(const Napi::CallbackInfo& /*info*/) {
        _flag->store(true);
    }

    Napi::Value Cancelled(const Napi::CallbackInfo& info) {
        return Napi::Boolean::New(info.Env(), _flag->load());
    }

    // Shared with the workers, which may outlive the JavaScript object
    std::shared_ptr<std::atomic<bool>> _flag;
};

class FeatureMapWorker : public PixelWorker {
public:
    FeatureMapWorker(Napi::Env env, const PixelArguments& arguments, std::string settings_json, int first_row, int row_count,
        std::shared_ptr<std::atomic<bool>> cancel)
        : PixelWorker(env, arguments),
          _settings_json(std::move(settings_json)),
          _first_row(first_row),
          _row_count(row_count),
          _cancel(std::move(cancel)) {}

    void Execute() override {
        try {
            _values = glcm::ComputeFeatureMapRows(
                Gray(), glcm::FeatureMapSettingsFromJson(_settings_json), _first_row, _row_count, _cancel.get());
        } catch (const glcm::FeatureMapCancelled& error) {
            Fail(CODE_CANCELLED, error.what());
        } catch (const std::invalid_argument& error) {
            Fail(CODE_INVALID_ARGUMENT, error.what());
        } catch (const std::exception& error) {
            Fail(CODE_INTERNAL, error.what());
        }
    }

    void OnOK() override {
        Napi::Float32Array values = Napi::Float32Array::New(Env(), _values.size());
        if (!_values.empty()) {
            std::memcpy(values.Data(), _values.data(), _values.size() * sizeof(float));
        }
        Resolve(values);
    }

private:
    std::string _settings_json;
    int _first_row;
    int _row_count;
    std::shared_ptr<std::atomic<bool>> _cancel;
    std::vector<float> _values;
};

// featureMapGrid(settingsJson, width, height): {step, columns, rows}; throws an Error with code INVALID_ARGUMENT when the
// settings are invalid for an image of this size
Napi::Value FeatureMapGridInfo(const Napi::CallbackInfo& info) {
    const std::string settings_json = StringArgument(info, 0, "settingsJson");
    const int width = IntegerArgument(info, 1, "width");
    const int height = IntegerArgument(info, 2, "height");
    Napi::Env env = info.Env();
    try {
        const glcm::FeatureMapSettings settings = glcm::FeatureMapSettingsFromJson(settings_json);
        glcm::ValidateFeatureMapSettings(settings);
        const glcm::FeatureMapGrid grid = glcm::ResolveFeatureMapGrid(width, height, settings.step);
        Napi::Object result = Napi::Object::New(env);
        result.Set("step", grid.step);
        result.Set("columns", grid.columns);
        result.Set("rows", grid.rows);
        result.Set("workPerRow", glcm::FeatureMapRowWork(settings, width, height));
        return result;
    } catch (const std::invalid_argument& error) {
        throw ErrorWithCode(env, CODE_INVALID_ARGUMENT, error.what());
    } catch (const std::exception& error) {
        throw ErrorWithCode(env, CODE_INTERNAL, error.what());
    }
}

// computeFeatureMap(pixels, width, height, bitDepth, settingsJson, firstRow, rowCount, cancelToken?): Promise<Float32Array> of
// rowCount × columns values (glcm::ComputeFeatureMapRows); rejects with code CANCELLED once the token is cancelled
Napi::Value ComputeFeatureMap(const Napi::CallbackInfo& info) {
    const PixelArguments pixels = ReadPixelArguments(info, 0);
    std::shared_ptr<std::atomic<bool>> cancel;
    if (info.Length() > 7 && !info[7].IsUndefined()) {
        if (!info[7].IsObject() || !info[7].As<Napi::Object>().CheckTypeTag(&CANCEL_TOKEN_TAG)) {
            throw Napi::TypeError::New(info.Env(), "cancelToken must be a CancelToken");
        }
        cancel = CancelToken::Unwrap(info[7].As<Napi::Object>())->Flag();
    }
    auto* worker = new FeatureMapWorker(info.Env(), pixels, StringArgument(info, 4, "settingsJson"), IntegerArgument(info, 5, "firstRow"),
        IntegerArgument(info, 6, "rowCount"), std::move(cancel));
    const Napi::Promise promise = worker->Promise();
    worker->Queue();
    return promise;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("coreVersion", Napi::Function::New(env, CoreVersion, "coreVersion"));
    exports.Set("catalog", Napi::Function::New(env, Catalog, "catalog"));
    exports.Set("decodeImageFile", Napi::Function::New(env, DecodeImageFile, "decodeImageFile"));
    exports.Set("renderDisplay", Napi::Function::New(env, RenderDisplay, "renderDisplay"));
    exports.Set("roiStats", Napi::Function::New(env, RoiStats, "roiStats"));
    exports.Set("validateAnalysis", Napi::Function::New(env, ValidateAnalysis, "validateAnalysis"));
    exports.Set("runAnalysis", Napi::Function::New(env, RunAnalysis, "runAnalysis"));
    exports.Set("formatResults", Napi::Function::New(env, FormatResults, "formatResults"));
    exports.Set("exportRoiImages", Napi::Function::New(env, ExportRoiImages, "exportRoiImages"));
    exports.Set("windowLevel", Napi::Function::New(env, WindowLevel, "windowLevel"));
    exports.Set("featureMapGrid", Napi::Function::New(env, FeatureMapGridInfo, "featureMapGrid"));
    exports.Set("computeFeatureMap", Napi::Function::New(env, ComputeFeatureMap, "computeFeatureMap"));
    exports.Set("CancelToken", CancelToken::Define(env));
    return exports;
}

} // namespace

NODE_API_MODULE(glcm_native, Init)
