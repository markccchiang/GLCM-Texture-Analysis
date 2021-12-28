#ifndef RECT_CONTROLLER_HPP_
#define RECT_CONTROLLER_HPP_

#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp> // selectROI is part of tracking API

#include "analysis/TextureAnalysis.hpp"
#include "viewer/Viewer.hpp"

namespace Rect {

class Controller {
public:
    Controller(){};
    ~Controller() = default;

    void Run(const std::string& filename, int d = 1, int Ng = 256);
};

} // namespace Rect

#endif // RECT_CONTROLLER_HPP_
