#include <iostream>
#include <opencv2/opencv.hpp>

#include "analysis/TextureAnalysis.hpp"
#include "viewer/Viewer.hpp"

using namespace std;
using namespace cv;

const int Ng = 256; // gray scale total number (0~255)
const int white_color = 255;
const int black_color = 0;

// Globals
bool finish_drawing = false; // finish drawing the polygon
bool execution = true;       // stop the program execution

cv::Mat drawing_image;  // drawing image
cv::Mat original_image; // original image
cv::Mat roi_image;      // ROI image
cv::Mat mask_image;     // Mask is black and white where our ROI is

std::vector<cv::Point> vertices; // polygon points
int img_width;                   // image width
int img_height;                  // image height
int d;                           // neighborhood distance

std::map<glcm::Type, glcm::Features> results; // GLCM calculation results

void MouseCallBackFunc(int event, int x, int y, int flags, void* userdata);
std::vector<std::pair<int, int>> GetMinMax(const std::vector<cv::Point>& vec);

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        cout << "Usage: ./glcm-polygon <file name> <distance>" << endl;
        return 1;
    }

    // Set the image file name
    string filename = argv[1];

    // Set the distance
    if (argc == 3) {
        string distance = argv[2];
        d = stoi(distance);
    } else {
        d = 1;
    }

    // Initialize the texture analysis
    glcm::TextureAnalysis texture_analysis(Ng);

    while (execution) {
        cout << "=================================================================================\n";
        cout << "Start new polygon drawing...\n";

        // Initialize the global variables
        drawing_image.release();
        original_image.release();
        roi_image.release();
        mask_image.release();
        vertices.clear();
        finish_drawing = false;

        // Read image from file
        drawing_image = imread(filename, IMREAD_GRAYSCALE);
        img_width = drawing_image.cols;
        img_height = drawing_image.rows;

        // Make a copy of the original image
        original_image = drawing_image.clone();

        // Create a window
        cv::namedWindow("Original Image");

        // Register a mouse callback
        cv::setMouseCallback("Original Image", MouseCallBackFunc, nullptr);

        // Main loop
        while (!finish_drawing) {
            cv::imshow("Original Image", drawing_image);
            // Check if ESC key was pressed
            if (cv::waitKey(20) == 27) {
                execution = false;
                break;
            }
        }

        if (roi_image.rows <= 0 || roi_image.cols <= 0) {
            break;
        }

        // Check the pixel values
        // cout << "\nROI = \n" << roi_image << endl << endl;                 // this has white lines
        // cout << "\ndrawing_image = \n" << drawing_image << endl << endl;   // this has white lines
        // cout << "\noriginal_image = \n" << original_image << endl << endl; // this is the original drawing_image that we want
        // cout << "\nmask_image = \n" << mask_image << endl << endl;         // this is the original mask_image that we want

        texture_analysis.ProcessPolygonImage(original_image, mask_image, d);

        // Set feature types to calculate
        std::set<glcm::Type> features{glcm::Type::Mean, glcm::Type::Entropy, glcm::Type::Contrast};

        // Clear the calculation results
        results.clear();

        // Re-calculate the features
        results = texture_analysis.Calculate(features);

        // Print results
        texture_analysis.Print(results);

        // Show results
        glcm::Viewer viewer(roi_image);
        viewer.DisplayScorePanel(&texture_analysis, results);
        // cv::waitKey(0);
    }

    // Print results
    texture_analysis.SaveAsCSV(filename, results, "glcm-polygon.csv");

    // Destroy all windows
    cv::destroyAllWindows();

    return 0;
}

void MouseCallBackFunc(int event, int x, int y, int flags, void* userdata) {
    // Right click the button to show the ROI
    if (event == EVENT_RBUTTONDOWN) {
        // cout << "Right mouse button clicked at (" << x << ", " << y << ")" << endl;
        if (vertices.size() < 2) {
            cerr << "You need a minimum of three points!" << endl;
            return;
        }

        // Close polygon (Scalar(0) is black color, will not show image if the mask_image in this part is black!)
        cv::line(drawing_image, vertices[vertices.size() - 1], vertices[0], Scalar(white_color), 1);

        // Mask is black with white where our ROI is
        mask_image = Mat::zeros(drawing_image.rows, drawing_image.cols, CV_8UC1);

        // Scalar(255) is white color (will show image in this part)
        std::vector<std::vector<cv::Point>> points{vertices};
        cv::fillPoly(mask_image, points, Scalar(white_color));

        // Copy the image to ROI with the mask_image with the white part (if value = 255)
        original_image.copyTo(roi_image, mask_image);

        // Get boundary points
        auto bounds = GetMinMax(vertices);
        // for (int i = 0; i < bounds.size(); ++i) {
        //    cout << "x = " << bounds[i].first << ", y = " << bounds[i].second << endl;
        //}

        roi_image(Rect(Point(bounds[0].first, bounds[0].second), Point(bounds[1].first, bounds[1].second))).copyTo(roi_image);

        finish_drawing = true;
        return;
    }

    // Left-click the button to draw a polygon
    if (event == EVENT_LBUTTONDOWN) {
        // cout << "Left mouse button clicked at (" << x << ", " << y << ")" << endl;
        if (x >= 0 && x <= img_width && y >= 0 && y <= img_height) {
            if (vertices.empty()) {
                // First click - just draw point
                // drawing_image.at<Vec3b>(y, x) = cv::Vec3b(white_color, 0, 0);
                drawing_image.at<uchar>(y, x) = white_color;
            } else {
                // Second, or later click, draw line to previous vertex
                cv::line(drawing_image, cv::Point(x, y), vertices[vertices.size() - 1], Scalar(white_color), 1);
            }
            vertices.push_back(cv::Point(x, y));
        }
        return;
    }
}

std::vector<std::pair<int, int>> GetMinMax(const std::vector<cv::Point>& vec) {
    int x_min = std::numeric_limits<int>::max();
    int y_min = std::numeric_limits<int>::max();
    int x_max = -std::numeric_limits<int>::min();
    int y_max = -std::numeric_limits<int>::min();

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
