#ifndef POLYGON_CONTROLLER_HPP_
#define POLYGON_CONTROLLER_HPP_

#include <iostream>
#include <opencv2/opencv.hpp>

#include "analysis/TextureAnalysis.hpp"
#include "viewer/Viewer.hpp"

namespace polygon {

class Controller {
public:
    Controller(){};
    ~Controller() = default;

    void Run(const std::string& filename, int d = 1, int Ng = 256);

private:
    static void MouseCallBackFunc(int event, int x, int y, int flags, void* userdata);
    static std::vector<std::pair<int, int>> GetMinMax(const std::vector<cv::Point>& vec);
};

} // namespace polygon

#endif // POLYGON_CONTROLLER_HPP_
