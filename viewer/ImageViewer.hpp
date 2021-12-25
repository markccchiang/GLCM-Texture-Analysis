#ifndef IMAGE_VIEWER_HPP_
#define IMAGE_VIEWER_HPP_

#include <iostream>
#include <opencv2/opencv.hpp>

#include "analysis/TextureAnalysis.hpp"

using namespace glcm;

class ImageViewer {
public:
    ImageViewer(const cv::Mat& image);
    ~ImageViewer() = default;

    void Display();
    void DisplayPanel();
    void DisplayScorePanel(glcm::TextureAnalysis* glcm_texture_analysis, std::map<Type, Features>& glcm_features);

private:
    cv::Mat _image;
};

#endif // IMAGE_VIEWER_HPP_
