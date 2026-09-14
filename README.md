# GLCM Texture Analysis

Computes Haralick texture features from the Gray Level Co-occurrence Matrix (GLCM) of regions of interest (ROIs) in grayscale images.

Open an 8- or 16-bit image in the browser, draw rectangle, ellipse, polygon or freehand ROIs, choose the features and GLCM settings (gray levels, quantization, distances, directions), and measure. Results appear in a table, per direction and aggregated, optionally with an age-based score.

The application has three parts:
- a C++ library (`core/`);
- a Node.js server that uses the library through a Node-API addon;
- a TypeScript web app (see [Web application](#web-application)).

## Requirements

- CMake 3.22 or newer
- A C++17 compiler
- [OpenCV](https://opencv.org/) (core, imgproc, imgcodecs). OpenCV 4 and 5 both work.
- [Eigen](https://eigen.tuxfamily.org/) 3.3 or newer (5.x works)
- [nlohmann/json](https://github.com/nlohmann/json) 3.11 or newer
- [GoogleTest](https://github.com/google/googletest) (optional, for the unit tests)
- [Node.js](https://nodejs.org/) 24 or newer (for the server and web app)

On macOS with Homebrew:

```bash
brew install cmake opencv eigen nlohmann-json googletest node
```

## Build

```bash
cmake -S . -B build
cmake --build build
```

This builds:

| Target | Description |
| --- | --- |
| `glcm_core` | Texture analysis library (`core/`): features, ROI masks, image loading, quantization, analysis pipeline, exports |
| `glcm-tests` | Unit tests (only if GoogleTest is found) |

To run the application, build the addon and web app and start the server; see [Web application](#web-application).

## Features

`glcm::TextureAnalysis` can compute these features:

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

## Web application

The web application (`doc/ui-design-plan.md`) has three parts:
- `web/` is the browser app (React, Mantine, Konva). It opens images, draws and manages ROIs, edits the analysis settings, measures, and shows the results table.
- `server/` is the API server. It uses the C++ core through a Node-API addon (`bindings/node`), runs analyses as jobs, and serves the built web app.
- `packages/api` holds the request and response schemas shared by both.

It needs Node.js 24 or newer, plus the C++ dependencies above.

```bash
npm install             # all workspaces
npm run build:native    # build the addon with cmake-js (again after changing core/)
npm run build:web       # build the web app into web/dist
npm start               # open http://127.0.0.1:8080/
npm test                # addon, server and web unit tests (Vitest)
npm run test:e2e        # end-to-end tests in Chromium and WebKit (Playwright; run `npx playwright install chromium webkit` once)
npm run typecheck       # TypeScript
npm run openapi         # regenerate packages/api/openapi.json
```

A typical session:
1. **Open an image:** use *File ▸ Open Image*, drag a file onto the window, or open a sample image.
2. **Draw ROIs:** use the rectangle (`R`), ellipse (`E`), polygon (`P`, double-click or `Enter` to close) or freehand (`F`) tool. Press `T` to add the drawn ROI to the ROI Manager.
   - The manager shows each ROI's pixel count, computed by the core with the same pixel-centre rule the analysis uses.
   - With the pointer tool, click to select (⌘/Ctrl or Shift to add), drag to move, and use the handles to resize or rotate. On a selected polygon, drag its vertices, double-click an edge to add a vertex, or Alt-click a vertex to remove it.
   - Arrow keys move selected ROIs (Shift: 10 px). `Z` zooms to the selection, and ⌘/Ctrl+Z undoes.
3. **Choose settings:** in *Analysis Settings*, choose a preset or features, gray levels, quantization, distances, directions and aggregation. The age-based score is under *Advanced*. Non-standard features are marked ⚠.
4. **Measure:** press `M` to measure the selected ROIs, or `⇧M` to measure all of them. Each ROI × distance pair is a job on the server; progress appears in the status bar, where the measurement can be cancelled.
5. **Review results:** rows are appended to the Results table and keep the settings they were computed with (hover a row to see them). You can sort, choose columns, and copy the table as tab-separated text.
6. **Save and export:**
   - *File ▸ Export Results as CSV / JSON* writes the table with the core's exporter. CSV files repeat the settings as `# key=value` lines, and non-standard columns end with ` [non-standard]`. Results with different settings are exported as a ZIP with one file per settings group.
   - *ROI ▸ Export ROI Set* writes `<image>.roi.json`. *Import ROI Set* adds its ROIs (undoable), warns when they were drawn on a different image, and clips ROIs to the image.
   - *ROI ▸ Export ROI Images* writes a ZIP. Each ROI gets its bounding-box crop (PNG, or TIFF for 16-bit images), a mask and, optionally, the quantized gray levels; `manifest.json` describes the geometry.
   - *File ▸ Save Project* (⌘/Ctrl+S) writes `<image>.glcmproj` with the ROIs, settings and results. The image is referenced by SHA-256 and can be embedded. *Open Project* finds the image on the server or re-uploads the embedded copy; otherwise it asks for the image file.
   - Projects and ROI sets can also be dropped onto the window.

For web development, run `npm start` and `npm run dev:web` side by side, then open http://127.0.0.1:5173/. The Vite dev server reloads on changes and forwards `/api` to port 8080.

Images up to 4096 × 4096 px are sent to the browser as raw samples and rendered there with a WebGL2 shader, which falls back to a lookup table. Its output is identical to the server's `display.png` rendering. Larger images are shown through `display.png`, and their pixel values come from `/pixel`.

Viewer controls:
- Wheel or pinch zooms; a trackpad two-finger scroll pans (configurable in *Edit ▸ Preferences*).
- Space + drag, a middle-button drag or the Pan tool pans.
- `+`/`−` zoom, `1` shows 100 %, `0` fits the image, arrow keys pan and `N` toggles the navigator.

On a loopback address the server runs in **local mode**, without authentication. Any other address is **server mode**, which requires an access token (`GLCM_API_TOKEN`); the web app asks for it once per browser tab. Server mode also switches to lower upload limits, rate limiting and retention. To deploy it with Docker behind an HTTPS reverse proxy, see [doc/deployment.md](doc/deployment.md), which also contains the security checklist.

```bash
export GLCM_API_TOKEN="$(openssl rand -base64 32)"
docker compose up -d                                  # server on 127.0.0.1:8080, data in the glcm-data volume
node scripts/smoke-test.mjs http://127.0.0.1:8080     # checks authentication, upload, analysis and export
```

The server is configured with environment variables:

| Variable | Default | Meaning |
| --- | --- | --- |
| `GLCM_HOST` | `127.0.0.1` | Listen address; anything other than `127.0.0.1`, `::1` or `localhost` is server mode |
| `GLCM_API_TOKEN` | none | Access token (at least 43 characters, e.g. `openssl rand -base64 32`); required in server mode, enforced whenever set |
| `GLCM_CORS_ORIGINS` | none | Comma-separated origins of other sites allowed to call the API |
| `GLCM_RATE_LIMIT_PER_MINUTE` | `0`; `600` in server mode | API requests per minute per token (0 = no limit) |
| `GLCM_RETENTION_HOURS` | `0`; `168` in server mode | Delete uploaded images and results older than this (0 = keep) |
| `GLCM_TRUST_PROXY` | `false` | Use `X-Forwarded-*` headers from a reverse proxy |
| `GLCM_PORT` | `8080` | Port |
| `GLCM_DATA_DIR` | `~/.glcm-texture-analysis`; `/data` in server mode | Uploaded images, results and caches |
| `GLCM_MAX_UPLOAD_BYTES` | 209,857,600 (200 MiB); 100 MiB in server mode | Largest upload |
| `GLCM_MAX_IMAGE_PIXELS` | 400,000,000; 100,000,000 in server mode | Largest decoded image |
| `GLCM_RAW_TRANSFER_MAX_PIXELS` | 16,777,216 (4096²) | Images up to this size are sent to the browser as raw data |
| `GLCM_DISPLAY_MAX_SIZE` | `4096` | Largest long side of `display.png` |
| `GLCM_DISPLAY_CACHE_BYTES` | 536,870,912 (512 MiB) | Disk space for cached `display.png` renderings |
| `GLCM_ANALYSIS_CONCURRENCY` | number of CPU cores | Analysis jobs (ROI × distance) running at the same time |
| `GLCM_WEB_DIR` | `web/dist` | Built web app |
| `GLCM_SAMPLES_DIR` | `samples/` | Sample images offered on the start screen |
| `GLCM_LOG_LEVEL` | `info` | Fastify log level |

`npm start` sets `UV_THREADPOOL_SIZE` to 16 unless it is already set. Analyses run in Node's libuv thread pool, which has only 4 threads by default.

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
| `POST /api/v1/images/{id}/roi-stats` | Pixel count, bounding box, min/max/mean/STD of ROIs |
| `POST /api/v1/analyses` | Start an analysis (image id, ROIs, settings); returns `202` |
| `GET`, `DELETE /api/v1/analyses/{id}` | Status; cancel (queued jobs are dropped) |
| `GET /api/v1/analyses/{id}/events` | Server-Sent Events: `result`, `progress`, `finished` |
| `GET /api/v1/analyses/{id}/results` | Results of the finished jobs (`glcm-results` JSON) |
| `GET /api/v1/analyses/{id}/results.csv`, `results.json` | The same results as a downloadable file written by the core |
| `POST /api/v1/exports/results` | CSV or JSON for a list of results documents (e.g. the current table); a ZIP when their settings differ |
| `POST /api/v1/exports/roi-images` | ZIP of ROI crops, masks, optional quantized images and `manifest.json` |
| `GET /api/v1/images?sha256` | Stored images, optionally by SHA-256 of the uploaded file (used when opening projects) |
| `GET /api/v1/images/{id}/original` | The uploaded file (used to embed images in projects) |
| `GET /api/v1/samples`, `GET /api/v1/samples/file?path` | Sample images |

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
| `bindings/node/` | Node-API addon (`@glcm/native`) exposing `glcm_core` to the server |
| `packages/api/` | Shared API schemas and types (`@glcm/api`) and the generated OpenAPI document |
| `server/` | Fastify API server (`@glcm/server`); also serves the built web app and the sample images |
| `web/` | Browser app (`@glcm/web`): React, Mantine, Konva, WebGL2 image rendering, ROI tools, settings and results |
| `e2e/` | Playwright end-to-end tests (`npm run test:e2e`), in local mode and with an access token |
| `Dockerfile`, `compose.yaml` | Server image and deployment example (`doc/deployment.md`) |
| `.github/workflows/` | CI: core, unit and end-to-end tests on macOS and Ubuntu; Docker image smoke test |
| `doc/` | Sphinx documentation (GLCM equations and references), the design plan and the deployment guide |
| `samples/` | Sample images: synthetic test patterns, CC0 textures and `lena.jpg` (see `samples/README.md`) |
| `scripts/` | Helper scripts, e.g. `generate-samples.ts` (`npm run samples`) |

## References

- R. M. Haralick, K. Shanmugam and I. Dinstein, "Textural Features for Image Classification," *IEEE Transactions on Systems, Man, and Cybernetics*, SMC-3(6), 1973.

See [`doc/references.rst`](doc/references.rst) (or the References page of the built documentation) for the full list.
