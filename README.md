# GLCM Texture Analysis

Interactive tool that computes Haralick texture features from the Gray Level Co-occurrence Matrix (GLCM) of a region you select in a grayscale image.

Select a rectangle or draw a polygon on the image. The tool computes the features in four directions (0°, 45°, 90° and 135°), shows intensity, entropy, contrast and an age-based score in a panel, and appends the results to a CSV file.

## Requirements

- CMake 3.22 or newer
- A C++17 compiler
- [OpenCV](https://opencv.org/) with the contrib modules (for `selectROI`). OpenCV 4 and 5 both work.
- [Eigen](https://eigen.tuxfamily.org/) 3.3 or newer (5.x works)
- [nlohmann/json](https://github.com/nlohmann/json) 3.11 or newer
- [GoogleTest](https://github.com/google/googletest) (optional, for the unit tests)
- [Node.js](https://nodejs.org/) 24 or newer (only for the web server)

On macOS with Homebrew:

```bash
brew install cmake opencv eigen nlohmann-json googletest
```

> OpenCV's contrib modules and HighGUI are only needed for the legacy desktop application. Configure with `-DGLCM_BUILD_LEGACY_APP=OFF` to build just the library and its tests.

## Build

```bash
cmake -S . -B build
cmake --build build
```

This builds:

| Target | Description |
| --- | --- |
| `glcm_core` | Texture analysis library (`core/`): features, ROI masks, image loading, quantization, analysis pipeline, exports |
| `glcm-analysis` | The (legacy) texture analysis desktop application |
| `glcm-tests` | Unit tests (only if GoogleTest is found) |
| `canvas-example` | A small [cvui](https://github.com/Dovyski/cvui) demo |

## Usage

```bash
./build/glcm-analysis <image> [rect|polygon] [distance]
```

| Argument | Default | Description |
| --- | --- | --- |
| `image` | – | Image file. It is read as 8-bit grayscale. |
| `rect` / `polygon` | `rect` | How the region is selected |
| `distance` | `1` | Pixel distance between the two pixels of a pair (≥ 1) |

Example:

```bash
./build/glcm-analysis samples/lena.jpg polygon 2
```

### Rectangle mode

1. Drag a rectangle on the image.
2. Press **Enter** or **Space** to confirm it. Press **c** to cancel the selection, which exits the program.
3. The score panel opens. Use the **Age** slider to update the score, and press **Esc** to close the panel.
4. Select the next region, or cancel to exit.

### Polygon mode

1. **Left-click** to add vertices.
2. **Right-click** to close the polygon (at least 3 vertices).
3. The score panel opens. Use the **Age** slider to update the score, and press **Esc** to close the panel.
4. Draw the next polygon, or press **Esc** on the image to exit.

Only pixel pairs whose two pixels are both inside the polygon are counted.

### Output

- The selected features are printed to the terminal for each direction (H = 0°, RD = 45°, V = 90°, LD = 135°) and their average.
- After each score panel is closed, one row per direction plus an average row is appended to `glcm-analysis.csv` in the current working directory. The header is written only when the file is created, so delete or rename the file if you change the selected features.

## Features

`glcm::TextureAnalysis` can compute these features (the application uses Mean, Entropy and Contrast):

| Group | Features |
| --- | --- |
| Region statistics | Mean, STD |
| Haralick | Energy (Angular Second Moment), Contrast, Correlation (I, II, III), Sum of Squares (in i, j, both), Homogeneity I, Homogeneity II (Inverse Difference Moment), Sum Average, Sum Variance, Sum Entropy, Entropy, Difference Variance, Difference Entropy, Information Measures of Correlation I and II, Maximal Correlation Coefficient |
| Others | Auto Correlation, Cluster Shade, Cluster Prominence, Dissimilarity, Maximum Probability, Inverse Difference Normalized, Inverse Difference Moment Normalized |

"Another way" variants of Contrast and Correlation compute the same value with a different formula and are useful as cross-checks.

Using the library directly (include paths are relative to `core/`):

```cpp
#include "imaging/ImageLoader.hpp"
#include "io/ResultsCsv.hpp"
#include "pipeline/AnalysisRunner.hpp"

glcm::LoadedImage image = glcm::LoadImageFile("samples/lena.jpg");

glcm::AnalysisSettings settings = glcm::DefaultSettings(image.info.bit_depth); // Haralick F1–F14, Ng = 32
settings.distances = {1, 2};

glcm::Roi roi;
roi.name = "ROI 1";
roi.shape = glcm::EllipseRoi{256.0, 256.0, 40.0, 25.0, 30.0}; // cx, cy, rx, ry, angle in degrees

glcm::AnalysisOutput output = glcm::RunAnalysis(image.gray, {roi}, settings);
double contrast_0_deg = output.results[0].values.at(glcm::Type::Contrast).H;
std::string csv = glcm::ResultsToCsv(output.results, settings, {"lena.jpg", "", "2026-09-14T12:00:00Z"});
```

Link against `glcm_core` (for example `target_link_libraries(my_app PRIVATE glcm_core)` after `add_subdirectory(core)`).

## Tests

```bash
ctest --test-dir build
./build/glcm-tests --gtest_filter='TextureAnalysisTest.ConstantImage'   # a single test
```

The tests (`core/tests/`) check the features against Haralick's worked example and against a simple, independent GLCM implementation, and cover ROI masks, image loading, quantization, display rendering, the analysis pipeline and the exporters.

## Web server (in development)

`server/` is the API server of the planned web application (`doc/ui-design-plan.md`, phase 1):
- it uses the C++ core through a Node-API addon (`bindings/node`);
- it shares its request and response schemas with the future web app through `packages/api`.

It needs Node.js 24 or newer, plus the C++ dependencies above.

```bash
npm install             # all workspaces
npm run build:native    # build the addon with cmake-js (again after changing core/)
npm test                # addon and server tests (Vitest)
npm run typecheck       # TypeScript
npm start               # http://127.0.0.1:8080/api/v1/health
npm run openapi         # regenerate packages/api/openapi.json
```

For now the server only listens on a loopback address; token authentication for server deployments comes in a later phase. It is configured with environment variables:

| Variable | Default | Meaning |
| --- | --- | --- |
| `GLCM_HOST` | `127.0.0.1` | Listen address; must be `127.0.0.1`, `::1` or `localhost` for now |
| `GLCM_PORT` | `8080` | Port |
| `GLCM_DATA_DIR` | `~/.glcm-texture-analysis` | Uploaded images and caches |
| `GLCM_MAX_UPLOAD_BYTES` | 209,857,600 (200 MiB) | Largest upload |
| `GLCM_MAX_IMAGE_PIXELS` | 400,000,000 | Largest decoded image |
| `GLCM_RAW_TRANSFER_MAX_PIXELS` | 16,777,216 (4096²) | Images up to this size are sent to the browser as raw data |
| `GLCM_DISPLAY_MAX_SIZE` | `4096` | Largest long side of `display.png` |
| `GLCM_DISPLAY_CACHE_BYTES` | 536,870,912 (512 MiB) | Disk space for cached `display.png` renderings |
| `GLCM_LOG_LEVEL` | `info` | Fastify log level |

Endpoints (full details in `packages/api/openapi.json`):

| Method and path | Purpose |
| --- | --- |
| `GET /api/v1/health` | Liveness and core version |
| `GET /api/v1/catalog` | Features (with non-standard flags), presets and limits |
| `POST /api/v1/images` | Upload an image as a multipart `file` field |
| `GET`, `DELETE /api/v1/images/{id}` | Image info (size, bit depth, default window, histogram); delete |
| `GET /api/v1/images/{id}/display.png?min&max&maxSize` | 8-bit rendering with window/level |
| `GET /api/v1/images/{id}/raw` | Raw little-endian samples, zstd or gzip compressed, for images up to 4096 × 4096 |
| `GET /api/v1/images/{id}/pixel?x&y` | One pixel value |

## Documentation

The `doc/` folder contains a [Sphinx](https://www.sphinx-doc.org/) site (theme: [sphinx_rtd_theme](https://sphinx-rtd-theme.readthedocs.io/)) with every GLCM equation as implemented in `core/analysis/TextureAnalysis.cpp` and a list of references.

Build it in a Python virtual environment (requires Python 3):

```bash
cd doc
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
make html
```

Open the generated pages in a browser:

```bash
open _build/html/index.html        # macOS
xdg-open _build/html/index.html    # Linux
```

To rebuild later, activate the environment again with `source .venv/bin/activate` and run `make html`. Use `make clean` to remove the generated pages. The equations are rendered with MathJax, which is loaded from a CDN, so viewing them needs an internet connection.

## Project structure

| Path | Contents |
| --- | --- |
| `core/` | `glcm_core` library: `analysis/` (GLCM features), `roi/` (ROI masks), `imaging/` (loading, quantization, display), `pipeline/` (settings, analysis runner), `io/` (JSON, CSV, ROI image export), `tests/` |
| `controller/`, `viewer/` | Legacy desktop application: selection loops and the cvui score panel (removed in phase 3 of `doc/ui-design-plan.md`) |
| `bindings/node/` | Node-API addon (`@glcm/native`) exposing `glcm_core` to the server |
| `packages/api/` | Shared API schemas and types (`@glcm/api`) and the generated OpenAPI document |
| `server/` | Fastify API server (`@glcm/server`) |
| `doc/` | Sphinx documentation: GLCM equations and references |
| `samples/` | Sample image |

## References

- R. M. Haralick, K. Shanmugam and I. Dinstein, "Textural Features for Image Classification," *IEEE Transactions on Systems, Man, and Cybernetics*, SMC-3(6), 1973.

See [`doc/references.rst`](doc/references.rst) (or the References page of the built documentation) for the full list.
