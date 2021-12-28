#include "RectController.hpp"

using namespace std;
using namespace cv;

namespace Rect {

void Controller::Run(const std::string& filename, int d, int Ng) {
    cv::Mat image = imread(filename, IMREAD_GRAYSCALE);
    glcm::TextureAnalysis texture_analysis(Ng);
    std::map<glcm::Type, glcm::Features> results;

    while (true) {
        bool from_center(false);
        bool show_cross_hair(false);
        Rect2d selected_roi = selectROI(image, from_center, show_cross_hair);

        if (selected_roi.area() <= 0) {
            break;
        }

        cv::Mat image_crop = image(selected_roi);
        texture_analysis.ProcessRectImage(image_crop, d);

        std::set<glcm::Type> features{glcm::Type::Mean, glcm::Type::Entropy, glcm::Type::Contrast};

        results.clear();
        results = texture_analysis.Calculate(features);
        texture_analysis.Print(results);

        if (image_crop.cols > 0 && image_crop.rows > 0) {
            glcm::Viewer viewer(image_crop);
            viewer.DisplayScorePanel(&texture_analysis, results);
        } else {
            std::cerr << "Invalid ROI image!\n";
        }

        if (cv::waitKey(20) == 27) {
            break;
        }
    }

    texture_analysis.SaveAsCSV(filename, results, "glcm-analysis.csv");

    cv::destroyAllWindows();
}

} // namespace Rect
