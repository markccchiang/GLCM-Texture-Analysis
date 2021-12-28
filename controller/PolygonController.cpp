#include "PolygonController.hpp"

using namespace std;
using namespace cv;

namespace polygon {

const int white_color = 255;
const int black_color = 0;

bool finish_drawing = false; // finish drawing the polygon
bool execution = true;       // stop the program execution

cv::Mat drawing_image;  // drawing image
cv::Mat original_image; // original image
cv::Mat roi_image;      // ROI image
cv::Mat mask_image;     // Mask is black and white where our ROI is

std::vector<cv::Point> vertices; // polygon points
int image_width;                 // image width
int image_height;                // image height

void Controller::Run(const std::string& filename, int d, int Ng) {
    glcm::TextureAnalysis texture_analysis(Ng);
    std::map<glcm::Type, glcm::Features> results;

    while (execution) {
        // Initialize the global variables
        drawing_image.release();
        original_image.release();
        roi_image.release();
        mask_image.release();
        vertices.clear();
        finish_drawing = false;

        drawing_image = imread(filename, IMREAD_GRAYSCALE);
        original_image = drawing_image.clone();
        image_width = drawing_image.cols;
        image_height = drawing_image.rows;

        cv::namedWindow("Original Image");
        cv::setMouseCallback("Original Image", MouseCallBackFunc, nullptr);

        while (!finish_drawing) {
            cv::imshow("Original Image", drawing_image);
            if (cv::waitKey(20) == 27) { // Check if ESC key was pressed
                execution = false;
                break;
            }
        }

        if (roi_image.rows <= 0 || roi_image.cols <= 0) {
            std::cerr << "Invalid ROI image\n";
            break;
        }

        texture_analysis.ProcessPolygonImage(original_image, mask_image, d);

        results.clear();
        std::set<glcm::Type> features{glcm::Type::Mean, glcm::Type::Entropy, glcm::Type::Contrast};
        results = texture_analysis.Calculate(features);
        texture_analysis.Print(results);

        glcm::Viewer viewer(roi_image);
        viewer.DisplayScorePanel(&texture_analysis, results);
    }

    texture_analysis.SaveAsCSV(filename, results, "glcm-analysis.csv");
    cv::destroyAllWindows();
}

void Controller::MouseCallBackFunc(int event, int x, int y, int flags, void* userdata) {
    // Right-click the button to show the ROI
    if (event == EVENT_RBUTTONDOWN) {
        if (vertices.size() < 2) {
            cerr << "You need a minimum of three points!" << endl;
            return;
        }

        cv::line(drawing_image, vertices[vertices.size() - 1], vertices[0], Scalar(white_color), 1);
        mask_image = Mat::zeros(drawing_image.rows, drawing_image.cols, CV_8UC1); // Initialize as a black image

        std::vector<std::vector<cv::Point>> points{vertices};
        cv::fillPoly(mask_image, points, Scalar(white_color));

        // Copy the image to ROI with the mask_image with the white part (if value = 255)
        original_image.copyTo(roi_image, mask_image);

        auto bounds = GetMinMax(vertices);
        roi_image(Rect(Point(bounds[0].first, bounds[0].second), Point(bounds[1].first, bounds[1].second))).copyTo(roi_image);

        finish_drawing = true;
        return;
    }

    // Left-click the button to draw a polygon
    if (event == EVENT_LBUTTONDOWN) {
        if (x >= 0 && x <= image_width && y >= 0 && y <= image_height) {
            if (vertices.empty()) {                          // First click - just draw point
                drawing_image.at<uchar>(y, x) = white_color; // drawing_image.at<Vec3b>(y, x) = cv::Vec3b(white_color, 0, 0);
            } else {                                         // Second, or later click, draw line to previous vertex
                cv::line(drawing_image, cv::Point(x, y), vertices[vertices.size() - 1], Scalar(white_color), 1);
            }
            vertices.push_back(cv::Point(x, y));
        }
        return;
    }
}

std::vector<std::pair<int, int>> Controller::GetMinMax(const std::vector<cv::Point>& vec) {
    int x_min = std::numeric_limits<int>::max();
    int y_min = std::numeric_limits<int>::max();
    int x_max = std::numeric_limits<int>::min();
    int y_max = std::numeric_limits<int>::min();

    for (auto point : vec) {
        if (point.x < x_min) {
            x_min = point.x;
        }
        if (point.y < y_min) {
            y_min = point.y;
        }
        if (point.x > x_max) {
            x_max = point.x;
        }
        if (point.y > y_max) {
            y_max = point.y;
        }
    }
    return {{x_min, y_min}, {x_max, y_max}};
}

} // namespace polygon
