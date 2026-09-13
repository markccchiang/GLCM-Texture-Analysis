# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Interactive C++17 tool that computes Haralick GLCM (Gray Level Co-occurrence Matrix) texture features on a user-selected region of a grayscale image. The user draws a rectangle or polygon in an OpenCV window; features are printed, shown in a cvui panel with a "Score" computed from an age slider, and appended to a CSV file when the program exits.

## Build & Run

Dependencies: OpenCV (with contrib — `opencv2/tracking.hpp` is included for `selectROI`) and Eigen3 ≥ 3.3.

```bash
cmake -S . -B build && cmake --build build
./build/glcm-analysis <image> [rect|polygon] [distance]   # main app, defaults to rect, distance 1
ctest --test-dir build                          # unit tests (glcm-tests, built only if GoogleTest is found)
./build/glcm-tests --gtest_filter='TextureAnalysisTest.ConstantImage'   # single test
./build/canvas-example                         # cvui demo; loads ../samples/lena.jpg, so run from build/
```

- `CMakeLists.txt` hard-codes `CMAKE_OSX_ARCHITECTURES "arm64"`; remove or override it on other platforms.
- Every `.cpp` listed in `SOURCES` is compiled into each executable separately (there is no shared library target). When you add a source file, add it to `SOURCES`.
- Tests live in `tests/TextureAnalysisTest.cpp`. They check the features against a simple, independent GLCM implementation written inside the test file. Linting uses `.clang-tidy`, which enables only the `readability-*` checks, and formatting uses `.clang-format` (Google style, 4-space indent, 140-column limit, left pointer alignment).

## Naming conventions (enforced by .clang-tidy)

Namespaces are `lower_case`, and classes and structs are `CamelCase`. Functions are `CamelCase`, variables are `lower_case`, and global constants are `UPPER_CASE`. Private members take a `_` prefix (e.g. `_Ng`, `_directions`).

## Architecture

- **`analysis/TextureAnalysis` (`namespace glcm`)**: all the math. Construct it with `Ng` (the number of gray levels, normally 256). Then:
  1. `ProcessRectImage(crop, d)` or `ProcessPolygonImage(image, mask, d)` both call `Process()`. It counts pixel pairs at the two neighbor offsets of each direction, then `Normalize()` fills one `DirectionData` per direction in `_directions` (order H, V, LD = 135°, RD = 45°). Each `DirectionData` holds the normalized GLCM `p`, the marginal vectors `px`, `py`, `p_xpy` and `p_xny`, and precomputed means and STDs. In polygon mode, a pair counts only if both pixels have mask value 255.
  2. `Calculate(std::set<Type>)` dispatches to the `GetXxx(Features&)` methods and returns a `std::map<Type, Features>`. `Features` holds one value per direction (H/V/LD/RD) plus `Avg()`.
  3. `CalculateScore(age, map)` adds `Type::Score` and `Type::Age` using a linear model with hard-coded coefficients. It requires `Mean`, `Entropy` and `Contrast` to be present.
  4. `SaveAsCSV` appends rows (one per direction) to the CSV. It writes a header only when the file doesn't exist yet, so the columns come from the feature set of the first run.
  - To add a feature, update the `Type` enum, the `Calculate` switch, `TypeToString`, and add a `GetXxx` method.
- **`controller/` (`namespace rect` / `namespace polygon`)**: each `Controller::Run(filename, d, Ng)` runs the loop of selecting an ROI, then process → Calculate → Viewer, until ESC. Both append each region to `glcm-analysis.csv` after its score panel is closed. `PolygonController` keeps its drawing state in file-scope globals inside `namespace polygon`, because the OpenCV mouse callback is static (left-click adds vertices, right-click closes the polygon).
- **`viewer/Viewer` (`namespace glcm`)**: builds on cvui. `viewer/cvui.h` is a vendored single-header library, and `CVUI_IMPLEMENTATION` is defined in `Viewer.cpp` (and separately in `canvas-example.cpp`). Don't define it anywhere else in the same target. `DisplayScorePanel` calls `.at(Type::Mean/Entropy/Contrast/Score)`, so the feature set passed in must include Mean, Entropy and Contrast.
- **Entry point**: `glcm-analysis.cpp` parses the arguments and runs the selected controller.
- **`ImageJ-plugin-codes/GLCM_Texture7.java`**: a GLCM texture ImageJ plugin (Julio E. Cabrera), kept for reference. It is not part of the build.
