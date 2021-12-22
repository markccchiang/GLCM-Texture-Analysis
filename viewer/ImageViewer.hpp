#ifndef IMAGE_VIEWER_HPP_
#define IMAGE_VIEWER_HPP_

#include <iostream>
#include <opencv2/opencv.hpp>

class ImageViewer {
public:
    ImageViewer(cv::Mat image);
    ~ImageViewer() = default;

    void Display();
    void DisplayPanel();

private:
    cv::Mat _image;
};

#endif // IMAGE_VIEWER_HPP_
