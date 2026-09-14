#include "io/RoiImageExport.hpp"

#include <opencv2/imgcodecs.hpp>
#include <set>
#include <stdexcept>

#include "imaging/Quantizer.hpp"
#include "io/JsonConversions.hpp"
#include "pipeline/Version.hpp"

namespace glcm {

namespace {

using json_detail::Json;

const size_t MAX_FILE_NAME_LENGTH = 100;

std::vector<uchar> Encode(const std::string& extension, const cv::Mat& image) {
    std::vector<uchar> bytes;
    if (!cv::imencode(extension, image, bytes)) {
        throw std::runtime_error("Cannot encode a " + extension + " image");
    }
    return bytes;
}

std::string UniqueName(const std::string& base, std::set<std::string>& used) {
    std::string candidate = base;
    for (int suffix = 2; !used.insert(candidate).second; ++suffix) {
        candidate = base + "_" + std::to_string(suffix);
    }
    return candidate;
}

bool IsSafeCharacter(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
}

} // namespace

std::string SanitizeFileName(const std::string& name) {
    std::string result;
    for (unsigned char c : name) {
        if (result.size() == MAX_FILE_NAME_LENGTH) {
            break;
        }
        result += IsSafeCharacter(c) ? static_cast<char>(c) : '_';
    }
    if (!result.empty() && result[0] == '.') {
        result[0] = '_';
    }
    return result.empty() ? "roi" : result;
}

std::vector<ExportedFile> ExportRoiImages(
    const cv::Mat& gray, const std::vector<Roi>& rois, const AnalysisSettings& settings, const RoiImageExportOptions& options) {
    if (gray.empty() || gray.channels() != 1 || (gray.depth() != CV_8U && gray.depth() != CV_16U)) {
        throw std::invalid_argument("The image must be a non-empty 8- or 16-bit single-channel image");
    }
    const bool sixteen_bit = gray.depth() == CV_16U;

    std::vector<ExportedFile> files;
    std::set<std::string> used_names;
    Json entries = Json::array();

    for (const Roi& roi : rois) {
        Json entry = Json::object();
        entry["roi"] = json_detail::RoiToJson(roi);

        cv::Mat mask;
        try {
            mask = RasterizeMask(roi.shape, gray.size());
        } catch (const std::exception& error) {
            entry["skipped"] = true;
            entry["reason"] = error.what();
            entries.push_back(entry);
            continue;
        }
        const int pixel_count = CountMaskPixels(mask);
        if (pixel_count == 0) {
            entry["skipped"] = true;
            entry["reason"] = "The ROI contains no pixels";
            entries.push_back(entry);
            continue;
        }

        const std::string base = UniqueName(SanitizeFileName(roi.name.empty() ? roi.id : roi.name), used_names);
        const cv::Rect box = MaskBoundingBox(mask);
        const cv::Mat mask_crop = mask(box).clone();
        cv::Mat crop = gray(box).clone();
        crop.setTo(cv::Scalar(0), mask_crop == 0);

        std::string image_name;
        if (sixteen_bit) {
            image_name = base + ".tif";
            files.push_back({image_name, Encode(".tif", crop)});
        } else if (options.transparent_outside) {
            cv::Mat bgra;
            cv::merge(std::vector<cv::Mat>{crop, crop, crop, mask_crop}, bgra);
            image_name = base + ".png";
            files.push_back({image_name, Encode(".png", bgra)});
        } else {
            image_name = base + ".png";
            files.push_back({image_name, Encode(".png", crop)});
        }
        const std::string mask_name = base + "_mask.png";
        files.push_back({mask_name, Encode(".png", mask_crop)});

        entry["boundingBox"] = {{"x", box.x}, {"y", box.y}, {"width", box.width}, {"height", box.height}};
        entry["pixelCount"] = pixel_count;
        entry["image"] = image_name;
        entry["mask"] = mask_name;

        if (options.include_quantized) {
            try {
                const QuantizationResult quantized = Quantize(gray, mask, settings.gray_levels, settings.quantization);
                const std::string quantized_name = base + "_q" + std::to_string(settings.gray_levels) + ".png";
                files.push_back({quantized_name, Encode(".png", quantized.image(box).clone())});
                entry["quantized"] = quantized_name;
            } catch (const std::invalid_argument& error) {
                entry["quantized"] = nullptr;
                entry["quantizationError"] = error.what();
            }
        }
        entries.push_back(entry);
    }

    Json manifest = Json::object();
    manifest["format"] = "glcm-roi-images";
    manifest["version"] = 1;
    manifest["coreVersion"] = CORE_VERSION;
    if (options.include_quantized) {
        manifest["settings"] = json_detail::SettingsToJsonValue(settings);
    }
    manifest["entries"] = entries;

    const std::string text = manifest.dump(2);
    files.push_back({"manifest.json", std::vector<uchar>(text.begin(), text.end())});
    return files;
}

} // namespace glcm
