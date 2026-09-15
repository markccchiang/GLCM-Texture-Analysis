# Installing Texture Workbench

This guide builds Texture Workbench from source and starts it on your own computer. For what the application does, see the [README](README.md); for tests, configuration, deployment on a shared server and the programming interfaces, see [DEVELOPMENT.md](DEVELOPMENT.md).

## 1. Install the requirements

You need:

- CMake 3.22 or newer
- A C++17 compiler
- [OpenCV](https://opencv.org/) (core, imgproc, imgcodecs). OpenCV 4 and 5 both work.
- [Eigen](https://eigen.tuxfamily.org/) 3.3 or newer (5.x works)
- [nlohmann/json](https://github.com/nlohmann/json) 3.11 or newer
- [Node.js](https://nodejs.org/) 24 or newer, for the server and the web app
- [GoogleTest](https://github.com/google/googletest), only to run the C++ unit tests
- Python 3, only to build the documentation

**macOS** with [Homebrew](https://brew.sh/):

```bash
brew install cmake opencv eigen nlohmann-json googletest node
```

**Debian or Ubuntu** (install Node.js 24 separately, for example from [nodejs.org](https://nodejs.org/)):

```bash
sudo apt install cmake g++ libopencv-dev libeigen3-dev nlohmann-json3-dev libgtest-dev
```

## 2. Build and start the application

In the folder of this repository:

```bash
npm install             # the JavaScript packages of all workspaces
npm run build:native    # the C++ core and its Node.js addon (cmake-js); again after changing core/
npm run build:web       # the web app, into web/dist
npm start               # the server, on http://127.0.0.1:8080/
```

Open http://127.0.0.1:8080/ in your browser and click **Open sample image**. The server runs in local mode, without an access token, and keeps uploaded images and results in `~/.glcm-texture-analysis`. Stop it with Ctrl+C.

To run it on a server shared by several people instead, with Docker and an access token, see [Local mode, server mode and deployment](DEVELOPMENT.md#local-mode-server-mode-and-deployment) and [doc/deployment.md](doc/deployment.md).

## 3. Build the C++ library on its own (optional)

`npm run build:native` builds everything the application needs. To build the `glcm_core` library and its unit tests separately, for example to use the library in your own program:

```bash
cmake -S . -B build
cmake --build build
```

This builds:

| Target | Description |
| --- | --- |
| `glcm_core` | Texture analysis library (`core/`): features, ROI masks and ROI operations, image loading, quantization, analysis pipeline, feature maps, exports |
| `glcm-tests` | Unit tests (only if GoogleTest is found); run them with `ctest --test-dir build` |

Link your program against `glcm_core`, for example with `target_link_libraries(my_app PRIVATE glcm_core)` after `add_subdirectory(core)`. [DEVELOPMENT.md](DEVELOPMENT.md#using-the-c-library) has an example.

## 4. Build the documentation (optional)

The documentation in `doc/` is a [Sphinx](https://www.sphinx-doc.org/) site. Build it in a Python virtual environment:

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

Once built, the running application also serves the documentation at http://127.0.0.1:8080/docs/, and *Help ▸ Feature Equations* links to it. To rebuild later, activate the environment again with `source .venv/bin/activate` and run `make html`; `make clean` removes the generated pages. The equations are rendered with MathJax, which is loaded from a CDN, so viewing them needs an internet connection.

## Check the installation (optional)

```bash
ctest --test-dir build     # C++ unit tests (after step 3)
npm test                   # addon, server and web unit tests
```

End-to-end tests and the other developer commands are described in [DEVELOPMENT.md](DEVELOPMENT.md).

## Troubleshooting

- **`npm run build:native` cannot find OpenCV, Eigen or nlohmann/json:** install the requirements of step 1. On macOS, open a new terminal after installing with Homebrew so that CMake finds them.
- **`npm install` or `npm start` fails with a syntax or engine error:** check `node --version`; Node.js 24 or newer is needed.
- **The browser shows "Not found" at http://127.0.0.1:8080/:** the web app was not built; run `npm run build:web` and reload.
- **Port 8080 is already in use:** start the server on another port with `GLCM_PORT=8081 npm start`.
