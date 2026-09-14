#include "io/RoiImageExport.hpp"

#include <algorithm>
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

// A base name (base, base_2, base_3, ...) for which every file name base + suffix is still unused, so that e.g. the mask
// of ROI "a" (a_mask.png) and the image of ROI "a_mask" (a_mask.png) cannot collide. Registers the file names.
std::string UniqueBase(const std::string& base, const std::vector<std::string>& suffixes, std::set<std::string>& used) {
    std::string candidate = base;
    const auto taken = [&](const std::string& name) {
        return std::any_of(suffixes.begin(), suffixes.end(), [&](const std::string& suffix) { return used.count(name + suffix) > 0; });
    };
    for (int number = 2; taken(candidate); ++number) {
        candidate = base + "_" + std::to_string(number);
    }
    for (const std::string& suffix : suffixes) {
        used.insert(candidate + suffix);
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
    std::set<std::string> used_names = {"manifest.json"};
    Json entries = Json::array();

    // Files written per ROI: image, mask and optionally the quantized image
    const std::string image_extension = sixteen_bit ? ".tif" : ".png";
    const std::string quantized_suffix = "_q" + std::to_string(settings.gray_levels) + ".png";
    std::vector<std::string> suffixes = {image_extension, "_mask.png"};
    if (options.include_quantized) {
        suffixes.push_back(quantized_suffix);
    }

    for (const Roi& roi : rois) {
        Json entry = Json::object();
        entry["roi"] = json_detail::RoiToJson(roi);

        // Only the box around the ROI is rasterized
        CroppedMask cropped;
        try {
            cropped = RasterizeCroppedMask(roi.shape, gray.size());
        } catch (const std::exception& error) {
            entry["skipped"] = true;
            entry["reason"] = error.what();
            entries.push_back(entry);
            continue;
        }
        const int pixel_count = CountMaskPixels(cropped.mask);
        if (pixel_count == 0) {
            entry["skipped"] = true;
            entry["reason"] = "The ROI contains no pixels";
            entries.push_back(entry);
            continue;
        }

        const std::string base = UniqueBase(SanitizeFileName(roi.name.empty() ? roi.id : roi.name), suffixes, used_names);
        // Bounding box of the ROI's pixels, within the cropped mask and in image coordinates
        const cv::Rect local_box = MaskBoundingBox(cropped.mask);
        const cv::Rect box = local_box + cropped.box.tl();
        const cv::Mat mask_crop = cropped.mask(local_box).clone();
        cv::Mat crop = gray(box).clone();
        crop.setTo(cv::Scalar(0), mask_crop == 0);

        const std::string image_name = base + image_extension;
        if (sixteen_bit) {
            files.push_back({image_name, Encode(".tif", crop)});
        } else if (options.transparent_outside) {
            cv::Mat bgra;
            cv::merge(std::vector<cv::Mat>{crop, crop, crop, mask_crop}, bgra);
            files.push_back({image_name, Encode(".png", bgra)});
        } else {
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
                const QuantizationResult quantized = Quantize(gray(cropped.box), cropped.mask, settings.gray_levels, settings.quantization);
                const std::string quantized_name = base + quantized_suffix;
                files.push_back({quantized_name, Encode(".png", quantized.image(local_box).clone())});
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
