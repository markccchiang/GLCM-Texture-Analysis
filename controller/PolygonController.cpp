#include "PolygonController.hpp"

using namespace std;
using namespace cv;

namespace polygon {

namespace {

const int white_color = 255;
const char* const WINDOW_NAME = "Original Image";

} // namespace

void Controller::Run(const std::string& filename, int d, int Ng) {
    cv::Mat image = imread(filename, IMREAD_GRAYSCALE);
    if (image.empty()) {
        std::cerr << "Can not read the image: " << filename << "\n";
        return;
    }

    glcm::TextureAnalysis texture_analysis(Ng);
    std::map<glcm::Type, glcm::Features> results;

    _execution = true;
    while (_execution) {
        // Start a new polygon on a clean copy of the image
        _original_image = image;
        _drawing_image = image.clone();
        _roi_image.release();
        _mask_image.release();
        _vertices.clear();
        _finish_drawing = false;

        cv::namedWindow(WINDOW_NAME);
        cv::setMouseCallback(WINDOW_NAME, MouseCallBackFunc, this);

        while (!_finish_drawing) {
            cv::imshow(WINDOW_NAME, _drawing_image);
            if (cv::waitKey(20) == 27) { // Check if ESC key was pressed
                _execution = false;
                break;
            }
        }

        if (_roi_image.rows <= 0 || _roi_image.cols <= 0) {
            std::cerr << "Invalid ROI image\n";
            break;
        }

        texture_analysis.ProcessPolygonImage(_original_image, _mask_image, d);

        results.clear();
        std::set<glcm::Type> features{glcm::Type::Mean, glcm::Type::Entropy, glcm::Type::Contrast};
        results = texture_analysis.Calculate(features);
        texture_analysis.Print(results);

        glcm::Viewer viewer(_roi_image);
        viewer.DisplayScorePanel(&texture_analysis, results);

        // Save every ROI after its panel is closed, so the Score and Age from the panel are included
        texture_analysis.SaveAsCSV(filename, results, "glcm-analysis.csv");
    }

    // Destroying the windows also removes the callback that points to this controller
    cv::destroyAllWindows();
}

void Controller::MouseCallBackFunc(int event, int x, int y, int /*flags*/, void* userdata) {
    static_cast<Controller*>(userdata)->OnMouse(event, x, y);
}

void Controller::OnMouse(int event, int x, int y) {
    if (event == EVENT_LBUTTONDOWN) { // Left-click to add a vertex
        AddVertex(x, y);
    } else if (event == EVENT_RBUTTONDOWN) { // Right-click to close the polygon
        ClosePolygon();
    }
}

void Controller::AddVertex(int x, int y) {
    if (x < 0 || x >= _drawing_image.cols || y < 0 || y >= _drawing_image.rows) {
        return;
    }

    if (_vertices.empty()) { // First click - just draw point
        _drawing_image.at<uchar>(y, x) = white_color;
    } else { // Second, or later click, draw line to previous vertex
        cv::line(_drawing_image, cv::Point(x, y), _vertices.back(), Scalar(white_color), 1);
    }
    _vertices.push_back(cv::Point(x, y));
}

void Controller::ClosePolygon() {
    if (_vertices.size() < 3) {
        cerr << "You need a minimum of three points!" << endl;
        return;
    }

    cv::line(_drawing_image, _vertices.back(), _vertices.front(), Scalar(white_color), 1);

    // Mask is black with white where the polygon is
    _mask_image = Mat::zeros(_drawing_image.rows, _drawing_image.cols, CV_8UC1);
    std::vector<std::vector<cv::Point>> points{_vertices};
    cv::fillPoly(_mask_image, points, Scalar(white_color));

    // Copy the image pixels inside the polygon (mask value 255); the rest stays black
    _original_image.copyTo(_roi_image, _mask_image);

    // Crop to the polygon's bounding box, including the right-most and bottom-most vertices
    cv::Point top_left = _vertices.front();
    cv::Point bottom_right = _vertices.front();
    for (const auto& vertex : _vertices) {
        top_left.x = std::min(top_left.x, vertex.x);
        top_left.y = std::min(top_left.y, vertex.y);
        bottom_right.x = std::max(bottom_right.x, vertex.x);
        bottom_right.y = std::max(bottom_right.y, vertex.y);
    }
    _roi_image = _roi_image(cv::Rect(top_left, bottom_right + cv::Point(1, 1))).clone();

    _finish_drawing = true;
}

} // namespace polygon
