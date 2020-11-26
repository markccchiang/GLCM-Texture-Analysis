#include <iostream>
#include <opencv2/opencv.hpp>

#include "GLCM/TextureAnalysis.hpp"

using namespace std;
using namespace cv;

const int Ng = 256; // gray scale total number (0~255)
const int white_color = 255;
const int black_color = 0;

// Globals
bool finish_drawing = false;     // finish drawing the polygon
bool execution = true;           // stop the program execution
cv::Mat img;                     // original image
cv::Mat roi;                     // ROI image
cv::Mat mask;                    // Mask is black and white where our ROI is
std::vector<cv::Point> vertices; // polygon points
int img_width;                   // image width
int img_height;                  // image height
int d;                           // neighborhood distance

void MouseCallBackFunc(int event, int x, int y, int flags, void* userdata);
std::vector<std::pair<int, int>> GetMinMax(const std::vector<cv::Point>& vec);

int main(int argc, char* argv[]) {
    if (argc < 2) {
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

    // Initialize the texure analysis
    glcm::TextureAnalysis texture_analysis(Ng);

    while (execution) {
        cout << "=================================================================================\n";
        cout << "Start new polygon drawing...\n";

        // Initialize the global variables
        img.release();
        roi.release();
        mask.release();
        vertices.clear();
        finish_drawing = false;

        // Clear the cache
        texture_analysis.ResetCache();

        // Read image from file
        img = imread(filename, IMREAD_GRAYSCALE);
        img_width = img.cols;
        img_height = img.rows;

        // Make a copy of the original image
        cv::Mat original_img = img.clone();

        // Create a window
        cv::namedWindow("Original Image", 1);

        // Register a mouse callback
        cv::setMouseCallback("Original Image", MouseCallBackFunc, nullptr);

        // Main loop
        while (!finish_drawing) {
            cv::imshow("Original Image", img);
            // Check if ESC key was pressed
            if (cv::waitKey(20) == 27) {
                execution = false;
                break;
            }
        }

        if (roi.rows <= 0 || roi.cols <= 0) {
            break;
        }

        // Check the pixel values
        // cout << "\nROI = \n" << ROI << endl << endl;                   // this has white lines
        // cout << "\nimg = \n" << img << endl << endl;                   //  this has white lines
        // cout << "\noriginal_img = \n" << original_img << endl << endl; // this is the original img that we want
        // cout << "\nmask = \n" << mask << endl << endl;                 // this is the original mask that we want

        int masked = 0;
        int non_masked = 0;

        // Calculate matrices elements:
        // Central pixel coord (m ,n), where "m" is the row index, and "n" is the column index
        for (int m = 0; m < original_img.rows; ++m) {
            for (int n = 0; n < original_img.cols; ++n) {
                // Nearest neighborhood pixel coord (k ,l), where "k" is the row index, and "l" is the column index
                for (int k = m - d; k <= m + d; ++k) {
                    for (int l = n - d; l <= n + d; ++l) {
                        if ((k >= 0) && (l >= 0) && (k < original_img.rows) && (l < original_img.cols)) {
                            // Check is the nearest neighborhood pixel coord (k ,l) masked
                            int mask_pixel_value = (int)(mask.at<uchar>(k, l));
                            if (mask_pixel_value == white_color) {           // if nearest neighborhood pixel coord (k ,l) is not masked
                                int j = (int)(original_img.at<uchar>(m, n)); // I(m,n)
                                int i = (int)(original_img.at<uchar>(k, l)); // I(k,l)
                                if (((k - m) == 0) && (abs(l - n) == d)) {
                                    texture_analysis.CountElemH(i, j);
                                } else if ((((k - m) == d) && ((l - n) == -d)) || (((k - m) == -d) && ((l - n) == d))) {
                                    texture_analysis.CountElemRD(i, j);
                                } else if ((abs(k - m) == d) && (l - n == 0)) {
                                    texture_analysis.CountElemV(i, j);
                                } else if ((((k - m) == d) && ((l - n) == d)) || (((k - m) == -d) && ((l - n) == -d))) {
                                    texture_analysis.CountElemLD(i, j);
                                } else {
                                    // if ((m != k) || (n != l)) {
                                    //    cerr << "unknown element:" << endl;
                                    //    cerr << "central element: (m, n) = (" << m << "," << n << ")" << endl;
                                    //    cerr << "neighborhood element: (k, l) = (" << k << "," << l << ")" << endl;
                                    //}
                                }
                                ++non_masked;
                            } else {
                                // cerr << "masked coord: (k, l) = (" << k << "," << l << "), pixel value = " << mask_pixel_value << endl;
                                ++masked;
                            }
                        }
                    }
                }
            }
        }
        cerr << "masked num = " << masked << ", non-masked num = " << non_masked << endl;
        cout << "---------------------------------------------------------------------------------\n";

        // Normalize the matrices
        texture_analysis.Normalization();

        std::set<glcm::Type> features{glcm::Type::AutoCorrelation, glcm::Type::Contrast, glcm::Type::CorrelationI,
            glcm::Type::CorrelationII, glcm::Type::ClusterProminence, glcm::Type::ClusterShade, glcm::Type::Dissimilarity,
            glcm::Type::Energy, glcm::Type::Entropy, glcm::Type::HomogeneityI, glcm::Type::HomogeneityII, glcm::Type::MaximumProbability,
            glcm::Type::SumOfSquares, glcm::Type::SumAverage, glcm::Type::SumEntropy, glcm::Type::SumVariance,
            glcm::Type::DifferenceVariance, glcm::Type::DifferenceEntropy, glcm::Type::InformationMeasuresOfCorrelationI,
            glcm::Type::InformationMeasuresOfCorrelationII, glcm::Type::InverseDifferenceNormalized,
            glcm::Type::InverseDifferenceMomentNormalized};

        std::map<glcm::Type, glcm::Features> results = texture_analysis.Calculate(features);
        texture_analysis.Print(results);

        //
        // Show results
        //
        cv::namedWindow("ROI", 1);
        cv::imshow("ROI", roi);
        // cv::waitKey(0);
    }

    //
    // Destroy all windows
    //
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

        // Close polygon (Scalar(0) is black color, will not show image if the mask in this part is black!)
        cv::line(img, vertices[vertices.size() - 1], vertices[0], Scalar(white_color), 1);

        // Mask is black with white where our ROI is
        mask = Mat::zeros(img.rows, img.cols, CV_8UC1);

        // Get boundary points
        auto bounds = GetMinMax(vertices);
        // for (int i = 0; i < bounds.size(); ++i) {
        //    cout << "x = " << bounds[i].first << ", y = " << bounds[i].second << endl;
        //}

        // Scalar(255) is white color (will show image in this part)
        std::vector<std::vector<cv::Point>> pts{vertices};
        cv::fillPoly(mask, pts, Scalar(white_color));

        // Copy the image to ROI with the mask with the white part (if value = 255)
        img.copyTo(roi, mask);
        roi(Rect(Point(bounds[0].first, bounds[0].second), Point(bounds[1].first, bounds[1].second))).copyTo(roi);

        finish_drawing = true;
        return;
    }

    // Left click the button to draw a polygon
    if (event == EVENT_LBUTTONDOWN) {
        // cout << "Left mouse button clicked at (" << x << ", " << y << ")" << endl;
        if (x >= 0 && x <= img_width && y >= 0 && y <= img_height) {
            if (vertices.empty()) {
                // First click - just draw point
                img.at<Vec3b>(x, y) = cv::Vec3b(white_color, 0, 0);
            } else {
                // Second, or later click, draw line to previous vertex
                cv::line(img, cv::Point(x, y), vertices[vertices.size() - 1], Scalar(white_color), 1);
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