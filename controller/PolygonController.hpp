#ifndef POLYGON_CONTROLLER_HPP_
#define POLYGON_CONTROLLER_HPP_

#include <iostream>
#include <opencv2/opencv.hpp>

#include "analysis/TextureAnalysis.hpp"
#include "viewer/Viewer.hpp"

namespace polygon {

class Controller {
public:
    Controller() = default;
    ~Controller() = default;

    void Run(const std::string& filename, int d = 1, int Ng = 256);

private:
    // OpenCV mouse callback; userdata is the Controller that registered it
    static void MouseCallBackFunc(int event, int x, int y, int flags, void* userdata);
    void OnMouse(int event, int x, int y);
    void AddVertex(int x, int y);
    void ClosePolygon();

    bool _finish_drawing = false; // finish drawing the polygon
    bool _execution = true;       // keep drawing new polygons until ESC

    cv::Mat _original_image; // original image
    cv::Mat _drawing_image;  // original image with the polygon lines drawn on it
    cv::Mat _roi_image;      // polygon region, cropped to its bounding box
    cv::Mat _mask_image;     // black image, white inside the polygon

    std::vector<cv::Point> _vertices; // polygon points
};

} // namespace polygon

#endif // POLYGON_CONTROLLER_HPP_
