#include "analysis/TextureAnalysis.hpp"
#include "controller/PolygonController.hpp"
#include "controller/RectController.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    const string usage = "Usage: ./glcm-analysis <file name> [mode: rect/polygon] [distance >= 1]";

    if (argc < 2 || argc > 4) {
        cout << usage << endl;
        return 1;
    }

    string filename = argv[1];
    string mode = (argc >= 3) ? argv[2] : "rect";

    int distance = 1;
    if (argc == 4) {
        try {
            distance = stoi(argv[3]);
        } catch (const std::exception&) {
            distance = 0;
        }
    }

    if ((mode != "rect" && mode != "polygon") || distance < 1) {
        cout << usage << endl;
        return 1;
    }

    if (mode == "polygon") {
        polygon::Controller controller;
        controller.Run(filename, distance);
    } else {
        rect::Controller controller;
        controller.Run(filename, distance);
    }

    return 0;
}
