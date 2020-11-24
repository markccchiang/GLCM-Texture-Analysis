#include <math.h>

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
cv::Mat ROI;                     // ROI image
cv::Mat mask;                    // Mask is black and white where our ROI is
std::vector<cv::Point> vertices; // polygon points

void MouseCallBackFunc(int event, int x, int y, int flags, void* userdata);
void PrintResults(string title, glcm::Features f);

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Usage: ./glcm-polygon <file name> <distance>" << endl;
        return 1;
    }

    // Set the image file name
    string filename = argv[1];

    // Set the distance
    string distance = argv[2];
    int d = stoi(distance);

    // Initialize the texure analysis
    glcm::TextureAnalysis texture_analysis(Ng);

    while (execution) {
        cout << "=================================================================================\n";
        cout << "Start new polygon drawing...\n";

        // Initialize the global variables
        img.release();
        ROI.release();
        mask.release();
        vertices.clear();
        finish_drawing = false;

        // Clear the cache
        texture_analysis.ResetCache();

        // Read image from file
        img = imread(filename, IMREAD_GRAYSCALE);

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

        if (ROI.rows <= 0 || ROI.cols <= 0) {
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

        glcm::Features f1;
        texture_analysis.GetAngularSecondMoment(f1);
        PrintResults("1. Angular Second Moment:", f1);

        glcm::Features f2;
        texture_analysis.GetContrast(f2);
        PrintResults("2. Contrast:", f2);

        glcm::Features f3;
        texture_analysis.GetCorrelation(f3);
        PrintResults("3. Correlation:", f3);

        glcm::Features f4;
        texture_analysis.GetVariance(f4);
        PrintResults("4: Sum of Squares: Variance:", f4);

        glcm::Features f5;
        texture_analysis.GetInverseDifferenceMoment(f5);
        PrintResults("5. Inverse Difference Moment:", f5);

        glcm::Features f6;
        texture_analysis.GetSumAverage(f6);
        PrintResults("6. Sum Average:", f6);

        glcm::Features f7;
        texture_analysis.GetSumVariance(f7);
        PrintResults("7. Sum Variance:", f7);

        glcm::Features f8;
        texture_analysis.GetSumEntropy(f8);
        PrintResults("8. Sum Entropy:", f8);

        glcm::Features f9;
        texture_analysis.GetEntropy(f9);
        PrintResults("9. Entropy:", f9);

        glcm::Features f10;
        texture_analysis.GetDifferenceVariance(f10);
        PrintResults("10. Difference Variance:", f10);

        glcm::Features f11;
        texture_analysis.GetDifferenceEntropy(f11);
        PrintResults("11. Difference Entropy:", f11);

        glcm::Features f12;
        glcm::Features f13;
        texture_analysis.GetInformationMeasuresOfCorrelation(f12, f13);
        PrintResults("12. Information Measures of Correlation:", f12);
        PrintResults("13. Information Measures of Correlation:", f13);

        // cout << "Calculating Maximal Correlation Coefficient...\n";
        // glcm::Features f14;
        // texture_analysis.GetMaximalCorrelationCoefficient(f14);
        // PrintResults("14. Maximal Correlation Coefficient:", f14);

        //
        // Show results
        //
        cv::namedWindow("ROI", 1);
        cv::imshow("ROI", ROI);
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
        cv::line(img, vertices[vertices.size() - 1], vertices[0], Scalar(0));

        // Mask is black with white where our ROI is
        mask = Mat::zeros(img.rows, img.cols, CV_8UC1);

        std::vector<std::vector<cv::Point>> pts{vertices};

        // Scalar(255) is white color (will show image in this part)
        cv::fillPoly(mask, pts, Scalar(white_color));

        // Copy the image to ROI with the mask with the white part (if value = 255)
        img.copyTo(ROI, mask);

        finish_drawing = true;
        return;
    }

    // Left click the button to draw a polygon
    if (event == EVENT_LBUTTONDOWN) {
        // cout << "Left mouse button clicked at (" << x << ", " << y << ")" << endl;
        if (vertices.empty()) {
            // First click - just draw point
            img.at<Vec3b>(x, y) = cv::Vec3b(white_color, 0, 0);
        } else {
            // Second, or later click, draw line to previous vertex
            cv::line(img, cv::Point(x, y), vertices[vertices.size() - 1], Scalar(white_color), 2);
        }

        vertices.push_back(cv::Point(x, y));
        return;
    }
}

void PrintResults(string title, glcm::Features f) {
    cout << title << endl;
    cout << std::setw(30) << "f1_H (0 deg) = " << f.H << endl;
    cout << std::setw(30) << "f1_V (90 deg) = " << f.V << endl;
    cout << std::setw(30) << "f1_LD (135 deg) = " << f.LD << endl;
    cout << std::setw(30) << "f1_RD (45 deg) = " << f.RD << endl;
    cout << std::setw(30) << "avg = " << f.Avg() << endl;
}