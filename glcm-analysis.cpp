#include "analysis/TextureAnalysis.hpp"
#include "controller/PolygonController.hpp"
#include "controller/RectController.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: ./glcm-analysis <file name> <mode: polygon/rect>" << endl;
        return 1;
    }

    string filename = argv[1];
    string mode("rect");

    if (argc == 3) {
        mode = argv[2];
    }

    if (mode == "polygon") {
        polygon::Controller controller;
        controller.Run(filename);
    } else {
        rect::Controller controller;
        controller.Run(filename);
    }

    return 0;
}
