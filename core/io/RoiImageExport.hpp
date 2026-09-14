#ifndef GLCM_ROI_IMAGE_EXPORT_HPP_
#define GLCM_ROI_IMAGE_EXPORT_HPP_

#include <opencv2/core.hpp>
#include <string>
#include <vector>

#include "pipeline/AnalysisSettings.hpp"
#include "roi/Roi.hpp"

namespace glcm {

struct ExportedFile {
    std::string name; // file name without directories
    std::vector<uchar> bytes;
};

struct RoiImageExportOptions {
    bool transparent_outside = false; // 8-bit images: pixels outside the ROI are transparent (PNG alpha) instead of 0
    bool include_quantized = false;   // also export the quantized gray levels of the ROI (settings.gray_levels, quantization)
};

// For each ROI with at least one pixel, cropped to the mask's bounding box:
// - "<name>.png" (8-bit) or "<name>.tif" (16-bit): the image with pixels outside the ROI set to 0
// - "<name>_mask.png": the mask (255 inside)
// - "<name>_q<Ng>.png": the quantized gray levels, when requested
// plus "manifest.json" describing every ROI (geometry, bounding box, pixel count, file names, or why it was skipped).
// Names come from SanitizeFileName(roi name, or id if the name is empty). "_2", "_3", ... is appended until none of the
// ROI's file names is used by another ROI or by the manifest, so every exported file name is unique.
std::vector<ExportedFile> ExportRoiImages(const cv::Mat& gray, const std::vector<Roi>& rois, const AnalysisSettings& settings,
    const RoiImageExportOptions& options = RoiImageExportOptions());

// Keeps ASCII letters, digits, '-', '_' and '.', replaces other bytes with '_', never starts with '.', at most 100
// characters, and "roi" for an empty result
std::string SanitizeFileName(const std::string& name);

} // namespace glcm

#endif // GLCM_ROI_IMAGE_EXPORT_HPP_
