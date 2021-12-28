#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp> // selectROI is part of tracking API

#include "analysis/TextureAnalysis.hpp"
#include "viewer/Viewer.hpp"

using namespace std;
using namespace cv;

const int Ng = 256;
int d; // neighborhood distance

std::map<glcm::Type, glcm::Features> results; // GLCM calculation results

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

    // Read image
    cv::Mat image = imread(filename, IMREAD_GRAYSCALE);

    // Initialize the texture analysis
    glcm::TextureAnalysis texture_analysis(Ng);

    // Select ROI repeatedly
    while (true) {
        cout << "=================================================================================\n";
        // Select ROI
        bool from_center(false);
        bool show_crosshair(false);
        Rect2d selected_roi = selectROI(image, from_center, show_crosshair);

        // Check is the ROI region valid
        if (selected_roi.area() <= 0) {
            break;
        }

        cout << "\nROI corners: tl(" << selected_roi.tl().x << "," << selected_roi.tl().y << "), br(" << selected_roi.br().x << ","
             << selected_roi.br().y << ")" << endl;
        cout << "ROI width: " << selected_roi.size().width << ", high: " << selected_roi.size().height << ", area: " << selected_roi.area()
             << endl;

        // Crop image
        cv::Mat image_crop = image(selected_roi);

        // Calculate matrices elements:
        texture_analysis.ProcessRectImage(image_crop, d);

        // Set feature types to calculate
        std::set<glcm::Type> features{glcm::Type::Mean, glcm::Type::Entropy, glcm::Type::Contrast};

        // Clear the calculation results
        results.clear();

        // Re-calculate the features
        results = texture_analysis.Calculate(features);

        // Print results
        texture_analysis.Print(results);

        // Display Cropped Image
        if (image_crop.cols > 0 && image_crop.rows > 0) {
            glcm::Viewer viewer(image_crop);
            viewer.DisplayScorePanel(&texture_analysis, results);
        } else {
            std::cerr << "Invalid ROI image!\n";
        }

        // Check if ESC key was pressed
        if (cv::waitKey(20) == 27) {
            break;
        }
    } // End of the while loop

    // Save results as the CSV format
    texture_analysis.SaveAsCSV(filename, results, "glcm-rectangle.csv");

    // Destroy all windows
    cv::destroyAllWindows();

    return 0;
}
