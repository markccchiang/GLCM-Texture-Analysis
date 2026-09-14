// Node-API bindings for glcm_core (doc/ui-design-plan.md, phase 1).
//
// Long-running work (decoding, rendering) runs in Napi::AsyncWorker threads and returns promises. Rejected promises
// carry an error `code`: INVALID_ARGUMENT, UNSUPPORTED_IMAGE, DECODE_FAILED or INTERNAL_ERROR.

#include <napi.h>

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <opencv2/imgcodecs.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "imaging/DisplayRenderer.hpp"
#include "imaging/ImageLoader.hpp"
#include "io/Identifiers.hpp"
#include "pipeline/AnalysisSettings.hpp"
#include "pipeline/FeatureCatalog.hpp"
#include "pipeline/Version.hpp"

namespace {

const char CODE_INVALID_ARGUMENT[] = "INVALID_ARGUMENT";
const char CODE_UNSUPPORTED_IMAGE[] = "UNSUPPORTED_IMAGE";
const char CODE_DECODE_FAILED[] = "DECODE_FAILED";
const char CODE_INTERNAL[] = "INTERNAL_ERROR";

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

// CV_8UC1 or CV_16UC1 image from row-major little-endian bytes (copied, so the input may be unaligned)
cv::Mat FromLittleEndianBytes(const uint8_t* data, int width, int height, int bit_depth) {
    cv::Mat gray(height, width, bit_depth == 16 ? CV_16UC1 : CV_8UC1);
    const size_t bytes_per_sample = static_cast<size_t>(bit_depth / 8);
    const size_t line_bytes = static_cast<size_t>(width) * bytes_per_sample;
    for (int row = 0; row < height; ++row) {
        const uint8_t* source = data + line_bytes * static_cast<size_t>(row);
        uint8_t* target = gray.ptr<uint8_t>(row);
        if (bytes_per_sample == 1 || HostIsLittleEndian()) {
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

Napi::Array StringArray(Napi::Env env, const std::vector<std::string>& values) {
    Napi::Array array = Napi::Array::New(env, values.size());
    for (size_t i = 0; i < values.size(); ++i) {
        array.Set(static_cast<uint32_t>(i), Napi::String::New(env, values[i]));
    }
    return array;
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
        Napi::Error rejection = Napi::Error::New(Env(), error.Message());
        rejection.Set("code", Napi::String::New(Env(), _code.empty() ? CODE_INTERNAL : _code));
        _deferred.Reject(rejection.Value());
    }

private:
    Napi::Promise::Deferred _deferred;
    std::string _code;
};

class DecodeImageWorker : public PromiseWorker {
public:
    DecodeImageWorker(Napi::Env env, std::string path) : PromiseWorker(env), _path(std::move(path)) {}

    void Execute() override {
        try {
            glcm::LoadedImage image = glcm::LoadImageFile(_path);
            _info = image.info;
            _warnings = image.warnings;
            _statistics = glcm::ComputeDisplayStatistics(image.gray);
            _pixels = ToLittleEndianBytes(image.gray);
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
    glcm::ImageInfo _info;
    std::vector<std::string> _warnings;
    glcm::DisplayStatistics _statistics;
    std::vector<uint8_t> _pixels;
};

class RenderDisplayWorker : public PromiseWorker {
public:
    RenderDisplayWorker(
        Napi::Env env, Napi::Uint8Array pixels, int width, int height, int bit_depth, int window_min, int window_max, int max_size)
        : PromiseWorker(env),
          _pixels(Napi::Persistent(pixels)),
          _data(pixels.Data()),
          _width(width),
          _height(height),
          _bit_depth(bit_depth),
          _window_min(window_min),
          _window_max(window_max),
          _max_size(max_size) {}

    void Execute() override {
        try {
            const cv::Mat gray = FromLittleEndianBytes(_data, _width, _height, _bit_depth);
            const cv::Mat rendered = glcm::RenderWindowLevel(gray, _window_min, _window_max, _max_size);
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
    Napi::Reference<Napi::Uint8Array> _pixels; // keeps the JavaScript buffer alive while the worker reads it
    const uint8_t* _data;
    int _width;
    int _height;
    int _bit_depth;
    int _window_min;
    int _window_max;
    int _max_size;
    std::vector<uchar> _png;
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

// decodeImageFile(path: string): Promise<DecodedImage>
Napi::Value DecodeImageFile(const Napi::CallbackInfo& info) {
    if (info.Length() < 1 || !info[0].IsString()) {
        throw Napi::TypeError::New(info.Env(), "path must be a string");
    }
    auto* worker = new DecodeImageWorker(info.Env(), info[0].As<Napi::String>().Utf8Value());
    const Napi::Promise promise = worker->Promise();
    worker->Queue();
    return promise;
}

// renderDisplay(pixels, width, height, bitDepth, windowMin, windowMax, maxSize): Promise<Buffer> (PNG)
Napi::Value RenderDisplay(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsTypedArray() || info[0].As<Napi::TypedArray>().TypedArrayType() != napi_uint8_array) {
        throw Napi::TypeError::New(env, "pixels must be a Uint8Array or Buffer");
    }
    const Napi::Uint8Array pixels = info[0].As<Napi::Uint8Array>();
    const int width = IntegerArgument(info, 1, "width");
    const int height = IntegerArgument(info, 2, "height");
    const int bit_depth = IntegerArgument(info, 3, "bitDepth");
    const int window_min = IntegerArgument(info, 4, "windowMin");
    const int window_max = IntegerArgument(info, 5, "windowMax");
    const int max_size = IntegerArgument(info, 6, "maxSize");

    if (width <= 0 || height <= 0) {
        throw Napi::TypeError::New(env, "width and height must be positive");
    }
    if (bit_depth != 8 && bit_depth != 16) {
        throw Napi::TypeError::New(env, "bitDepth must be 8 or 16");
    }
    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(bit_depth / 8);
    if (pixels.ByteLength() != expected) {
        throw Napi::TypeError::New(
            env, "pixels has " + std::to_string(pixels.ByteLength()) + " bytes, expected " + std::to_string(expected));
    }

    auto* worker = new RenderDisplayWorker(env, pixels, width, height, bit_depth, window_min, window_max, max_size);
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

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("coreVersion", Napi::Function::New(env, CoreVersion, "coreVersion"));
    exports.Set("catalog", Napi::Function::New(env, Catalog, "catalog"));
    exports.Set("decodeImageFile", Napi::Function::New(env, DecodeImageFile, "decodeImageFile"));
    exports.Set("renderDisplay", Napi::Function::New(env, RenderDisplay, "renderDisplay"));
    exports.Set("windowLevel", Napi::Function::New(env, WindowLevel, "windowLevel"));
    return exports;
}

} // namespace

NODE_API_MODULE(glcm_native, Init)
