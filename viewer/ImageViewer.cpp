#include "ImageViewer.hpp"

#define CVUI_IMPLEMENTATION

#include "cvui.h"

#define WINDOW_NAME "GLCM Image Viewer"

ImageViewer::ImageViewer(
    const cv::Mat& image, const std::map<glcm::Type, glcm::Features>& glcm_features, glcm::TextureAnalysis* glcm_texture_analysis)
    : _image(image), _glcm_features(glcm_features), _glcm_texture_analysis(glcm_texture_analysis) {}

void ImageViewer::Display() {
    cv::imshow("Image", _image);
}

void ImageViewer::DisplayPanel() {
    cv::Mat frame = _image.clone();
    int low_threshold = 50;
    int high_threshold = 150;
    int panel_width = 180;
    int panel_height = 280;
    int pad = 15;
    bool use_canny = false;

    cvui::init(WINDOW_NAME);
    int canvas_width = frame.cols + panel_width + pad * 3;
    int canvas_height = std::max(frame.rows, panel_height) + pad * 2;
    int trackbar_width = 165;
    int items_x = frame.cols + pad * 2;
    int frame_x = pad;
    int frame_y = canvas_height / 2 - frame.rows / 2;

    while (true) {
        cv::Mat canvas = cv::Mat::zeros(cv::Size(canvas_width, canvas_height), CV_8UC1);

        // Should we apply Canny edge?
        if (use_canny) {
            // Yes, we should apply it.
            cv::Canny(_image, frame, low_threshold, high_threshold, 3);
        } else {
            // No, so just copy the original image to the displaying frame.
            _image.copyTo(frame);
        }

        // Render the settings window to house the checkbox and the trackbars below.
        cvui::window(canvas, items_x, pad, panel_width, panel_height, "Settings");

        // Checkbox to enable/disable the use of Canny edge
        cvui::checkbox(canvas, items_x + 5, pad * 3, "Use Canny Edge", &use_canny);

        // Two trackbars to control the low and high threshold values for the Canny edge algorithm.
        cvui::trackbar(canvas, items_x, pad * 5, trackbar_width, &low_threshold, 5, 150);
        cvui::trackbar(canvas, items_x, pad * 9, trackbar_width, &high_threshold, 80, 300);

        cvui::printf(canvas, items_x + 5, pad * 14, 1.3 * cvui::DEFAULT_FONT_SCALE, 0xffffff, "Low threshold: %d", low_threshold);
        cvui::printf(canvas, items_x + 5, pad * 16.5, 1.3 * cvui::DEFAULT_FONT_SCALE, 0xffffff, "High threshold: %d", high_threshold);

        // This function must be called *AFTER* all UI components. It does all the behind the scenes magic to handle mouse clicks, etc.
        cvui::update();

        // Show everything on the screen
        frame.copyTo(canvas(cv::Rect(frame_x, frame_y, frame.cols, frame.rows)));

        cv::imshow(WINDOW_NAME, canvas);

        // Check if ESC was pressed
        if (cv::waitKey(30) == 27) {
            break;
        }
    }
}
