#ifndef VIEWER_HPP_
#define VIEWER_HPP_

#include <iostream>
#include <opencv2/opencv.hpp>

#include "analysis/TextureAnalysis.hpp"

namespace glcm {

class Viewer {
public:
    Viewer(const cv::Mat& image);
    ~Viewer() = default;

    void Display();
    void DisplayPanel();
    void DisplayScorePanel(TextureAnalysis* glcm_texture_analysis, std::map<Type, Features>& glcm_features);

private:
    cv::Mat _image;
};

} // namespace glcm

#endif // VIEWER_HPP_
