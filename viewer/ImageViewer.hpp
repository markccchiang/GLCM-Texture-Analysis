#ifndef IMAGE_VIEWER_HPP_
#define IMAGE_VIEWER_HPP_

#include <iostream>
#include <opencv2/opencv.hpp>

#include "GLCM/TextureAnalysis.hpp"

class ImageViewer {
public:
    ImageViewer(
        const cv::Mat& image, const std::map<glcm::Type, glcm::Features>& glcm_features, glcm::TextureAnalysis* glcm_texture_analysis);
    ~ImageViewer() = default;

    void Display();
    void DisplayPanel();

private:
    cv::Mat _image;
    std::map<glcm::Type, glcm::Features> _glcm_features;
    glcm::TextureAnalysis* _glcm_texture_analysis = nullptr;
};

#endif // IMAGE_VIEWER_HPP_
