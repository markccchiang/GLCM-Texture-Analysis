.. _api:

APIs and file formats
=====================

The application has three programming interfaces: the HTTP API used by the web app, the Node.js addon used by the
server, and the C++ library. This chapter also describes the files the application reads and writes.

HTTP API
--------

Conventions
~~~~~~~~~~~

- **Base path:** ``/api/v1``, on the same origin as the web app (``http://127.0.0.1:8080`` in local mode).
- **Schemas:** every request and response is defined with TypeBox in ``packages/api/src``. Requests are validated and
  responses serialized with these schemas. The complete OpenAPI 3.1 document is generated into
  ``packages/api/openapi.json`` (``npm run openapi``).
- **Authentication:** when the server has an access token (always in server mode), every request except
  ``GET /health`` needs ``Authorization: Bearer <token>``. ``GET /health`` reports whether a token is needed.
- **Errors:** every 4xx and 5xx response has the body ``{"error": "<code>", "message": "<description>"}``.
- **Identifiers:** images are ``img_``, analyses ``ana_`` and feature maps ``fmap_`` followed by 32 hexadecimal digits.
- **Coordinates:** ROI coordinates are image pixels as floating-point numbers; pixel ``(column c, row r)`` covers
  ``[c, c + 1) × [r, r + 1)``.

Endpoints
~~~~~~~~~

.. list-table:: System
   :header-rows: 1
   :widths: 35 65

   * - Method and path
     - Description
   * - ``GET /health``
     - ``{status, coreVersion, mode: "local"|"server", authentication: "none"|"bearer"}``; never needs the token
   * - ``GET /catalog``
     - Features (id, name, group, non-standard flag and reason, documentation anchor, cost), presets, analysis limits
       and upload limits

.. list-table:: Images
   :header-rows: 1
   :widths: 35 65

   * - Method and path
     - Description
   * - ``POST /images``
     - Upload one file as ``multipart/form-data``; ``201`` with ``ImageInfo``. ``413`` too large, ``415`` not
       multipart, ``422`` ``InvalidImage``, ``UnsupportedImage`` or ``ImageTooLarge``
   * - ``GET /images?sha256=``
     - Stored images, newest first, optionally only those whose uploaded file has this SHA-256
   * - ``GET /images/{id}``, ``DELETE /images/{id}``
     - ``ImageInfo``; delete the image (``204``)
   * - ``GET /images/{id}/raw``
     - Grayscale samples for images with ``transfer: "raw"`` (``409 RawNotAvailable`` otherwise), compressed with zstd
       or gzip when accepted, ``ETag`` and immutable caching
   * - ``GET /images/{id}/display.png?min&max&maxSize``
     - 8-bit PNG with window/level, long side at most ``maxSize``; ``ETag`` and ``304``
   * - ``GET /images/{id}/pixel?x&y``
     - ``{x, y, value}`` for one pixel
   * - ``GET /images/{id}/original``
     - The uploaded file, as a download

.. list-table:: ROIs and analyses
   :header-rows: 1
   :widths: 35 65

   * - Method and path
     - Description
   * - ``POST /images/{id}/roi-stats``
     - ``{rois: [{id, shape}]}`` → ``{stats: [{roiId, pixelCount, boundingBox, min, max, mean, std, error}]}``
   * - ``POST /images/{id}/threshold-rois``
     - ``{min, max, minPixels, maxRegions}`` → ``{regions: [{points, pixelCount, boundingBox}], total}``: the 8-connected
       parts of the pixels in ``[min, max]`` with holes filled, outlined along the pixel edges, at least ``minPixels``
       pixels each, the largest ``maxRegions`` (at most 1000; 0 returns only ``total``)
   * - ``POST /images/{id}/wand-roi``
     - ``{x, y, tolerance}`` → ``{region}``: the 8-connected region around pixel ``(x, y)`` whose values differ from its
       value by at most ``tolerance``, outlined the same way; ``null`` outside the image
   * - ``POST /analyses``
     - ``{imageId, rois, settings}`` → ``202`` with ``AnalysisInfo``; ``400`` for invalid settings or ROIs; ``422``
       ``TooManyJobs`` when ROIs × distances exceed ``GLCM_MAX_PENDING_JOBS``; ``503`` ``ServerBusy`` (with
       ``Retry-After``) while the job queue is full
   * - ``GET /analyses/{id}``
     - ``AnalysisInfo``: status (``queued``, ``running``, ``completed``, ``cancelled``, ``failed``), completed and total
       jobs, settings
   * - ``DELETE /analyses/{id}``
     - Cancel: queued jobs are dropped, running jobs finish (``204``)
   * - ``GET /analyses/{id}/events``
     - Server-Sent Events, see :ref:`api-events`
   * - ``GET /analyses/{id}/results``
     - ``glcm-results`` document with the finished jobs, ordered by ROI and distance
   * - ``GET /analyses/{id}/results.csv``, ``.json``
     - The same results as a downloadable CSV or JSON file written by the core

.. list-table:: Feature maps
   :header-rows: 1
   :widths: 35 65

   * - Method and path
     - Description
   * - ``POST /feature-maps``
     - ``{imageId, settings}`` → ``202`` with ``FeatureMapInfo``; ``400`` for invalid settings, ``404`` for an unknown
       image; ``422`` ``TooManyJobs`` when the map needs more bands of rows (about a second of computing each) than
       ``GLCM_MAX_FEATURE_MAP_BANDS`` or ``GLCM_MAX_PENDING_JOBS``; ``503`` ``ServerBusy`` while the job queue is full
   * - ``GET /feature-maps/{id}``
     - ``FeatureMapInfo``: status, ``step``, ``columns``, ``rows``, ``completedRows``, ``error``, settings
   * - ``GET /feature-maps/{id}/values``
     - ``rows × columns`` little-endian 32-bit floats, row-major, NaN where a window has no pixel pairs;
       ``409 NotReady`` until the map has completed
   * - ``DELETE /feature-maps/{id}``
     - Cancel a queued or running map (queued bands are dropped, running bands stop after their current point), or
       forget a finished one (``204``)

.. list-table:: Exports and samples
   :header-rows: 1
   :widths: 35 65

   * - Method and path
     - Description
   * - ``POST /exports/results``
     - ``{format: "csv"|"json", documents: [{timestamp, image, settings, results}]}``. Documents with equal settings and
       image are merged into one file; several groups are returned as a ZIP with one file per group
   * - ``POST /exports/roi-images``
     - ``{imageId, rois, settings?, transparentOutside, includeQuantized}`` → ZIP of crops, masks, optional quantized
       images and ``manifest.json``
   * - ``GET /samples``, ``GET /samples/file?path=``
     - Sample images offered on the start screen, and one sample file

Main schemas
~~~~~~~~~~~~

**ImageInfo** — ``imageId``, ``name``, ``sizeBytes``, ``width``, ``height``, ``bitDepth`` (8 or 16),
``sourceChannels``, ``sha256``, ``transfer`` (``"raw"`` or ``"server"``), ``windowMin``, ``windowMax`` (0.5 and 99.5
percentiles), ``histogram`` (256 bins), ``pixelSpacing``, ``warnings``, ``createdAt``.

**Pixel spacing** — ``{x, y}`` in millimetres per pixel. ``ImageInfo.pixelSpacing`` comes from the file's resolution
(PNG ``pHYs``, JPEG JFIF, BMP, TIFF; ``null`` without one, or for 72/96 dpi on both axes). An ``AnalysisRequest`` may
set ``pixelSpacing`` (a spacing, or ``null`` for none) to override it; ``AnalysisInfo.pixelSpacing`` records the value
used, and the results document carries it as ``image.pixelSpacing``. Result documents with a spacing export an
``areaMm2`` column and a ``# pixelSpacingMm=x;y`` comment; ``POST /exports/results`` groups documents by spacing too.
Features never depend on the spacing.

**ROI** — ``{id, name, color?, shape}`` where ``shape`` is one of:

.. code-block:: text

   {"type": "rectangle", "x": 100, "y": 100, "width": 64, "height": 64}
   {"type": "ellipse", "cx": 260.5, "cy": 300, "rx": 40, "ry": 25, "angle": 30}
   {"type": "polygon", "points": [[10, 10], [80, 20], [40, 90]], "freehand": false}

A request may contain up to 1000 ROIs; a polygon up to 10 000 vertices.

**AnalysisSettings**

.. code-block:: json

   {
     "features": ["Contrast", "Entropy", "CorrelationII", "CorrelationIII"],
     "grayLevels": 32,
     "quantization": {"method": "fixedRange", "min": 0, "max": 65535, "binWidth": 0},
     "distances": [1, 2],
     "directions": [0, 45, 90, 135],
     "aggregation": "perDirectionAndMean",
     "logBase": "natural",
     "score": {"enabled": true, "age": 40, "coefficients": [1.138, -1.814, 1.416, 1.714],
               "profile": "calibration", "intensityMin": 0, "intensityMax": 65535}
   }

- ``features``: ids from ``GET /catalog``.
- ``grayLevels``: 2–256.
- ``quantization.method``: ``fixedRange``, ``roiMinMax``, ``fixedBinWidth`` or ``none``.
- ``distances``: 1–64.
- ``aggregation``: ``perDirectionAndMean``, ``meanOnly`` or ``meanAndRange``.
- ``logBase``: ``natural`` or ``log2``.
- ``score.profile``: ``calibration`` computes the score inputs with Ng = 256, d = 1 and all directions whatever the
  settings; ``currentSettings`` uses the settings and warns.

**MeasurementResult** — one ROI at one distance:

.. code-block:: json

   {
     "roiId": "7f3c", "roiName": "ROI 1", "distance": 1,
     "status": "ok", "error": "", "pixelCount": 4096,
     "pairCounts": {"0": 8064, "45": 7938, "90": 8064, "135": 7938},
     "quantization": {"lower": 0, "upper": 255},
     "values": {"Contrast": {"0": 2.08, "45": 3.11, "90": 1.95, "135": 3.02, "mean": 2.54, "range": 1.16}},
     "score": null,
     "warnings": []
   }

Values are ``null`` for directions that were not selected. ``status`` is ``skipped`` (e.g. fewer than 2 pixels) or
``failed`` (e.g. intensities outside the gray levels without quantization) with ``error`` explaining why.

.. _api-events:

Progress events
~~~~~~~~~~~~~~~

``GET /analyses/{id}/events`` returns ``text/event-stream``. Results of jobs that finished before the request are
replayed first, so a client that connects late receives everything. The stream ends after ``finished``:

.. code-block:: text

   event: result
   data: {"index": 0, "result": { MeasurementResult }}

   event: progress
   data: {"completed": 1, "total": 4}

   event: finished
   data: {"status": "completed", "completed": 4, "total": 4, "error": null}

``index`` is the position of the result in the final order (ROI index × number of distances + distance index). Lines
starting with ``:`` are keep-alive comments.

Raw pixel data
~~~~~~~~~~~~~~

The body of ``GET /images/{id}/raw`` is the grayscale samples row by row from the top-left pixel, with no header;
16-bit samples are little-endian. The size is in the headers ``X-Image-Width``, ``X-Image-Height``,
``X-Image-Bit-Depth`` and ``X-Image-Byte-Order: little-endian``. Clients should check that the decoded length equals
``width × height × bitDepth / 8``.

Error codes
~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 12 28 60

   * - Status
     - ``error``
     - Meaning
   * - 400
     - ``BadRequest``
     - The request does not match the schema, or the core rejected the settings or ROIs
   * - 401
     - ``Unauthorized``
     - Missing or wrong access token (with ``WWW-Authenticate: Bearer``)
   * - 404
     - ``NotFound``
     - Unknown image, analysis, sample or route
   * - 409
     - ``RawNotAvailable``
     - Raw samples requested for a large image; use ``display.png`` and ``/pixel``
   * - 413
     - ``PayloadTooLarge``
     - Upload larger than ``GLCM_MAX_UPLOAD_BYTES``
   * - 415
     - ``UnsupportedMediaType``
     - Upload not sent as ``multipart/form-data``
   * - 422
     - ``InvalidImage``, ``UnsupportedImage``, ``ImageTooLarge``, ``TooManyJobs``
     - The file cannot be decoded, has an unsupported format (e.g. 32-bit float), or has too many pixels (checked from
       the file header before decoding); an analysis has more jobs than the server allows
   * - 429
     - ``TooManyRequests``
     - Rate limit exceeded (with ``Retry-After``)
   * - 503
     - ``ServerBusy``
     - The analysis job queue is full (with ``Retry-After``)
   * - 500
     - ``InternalError``
     - Unexpected server error (details only in the server log)

Example session
~~~~~~~~~~~~~~~

.. code-block:: bash

   API=http://127.0.0.1:8080/api/v1
   AUTH="Authorization: Bearer $GLCM_API_TOKEN"     # only needed when the server has a token

   curl -s $API/health
   IMAGE=$(curl -s -H "$AUTH" -F file=@samples/textures/brick.png $API/images | jq -r .imageId)

   ANALYSIS=$(curl -s -H "$AUTH" -H 'Content-Type: application/json' $API/analyses -d '{
     "imageId": "'$IMAGE'",
     "rois": [{"id": "r1", "name": "ROI 1", "shape": {"type": "rectangle", "x": 10, "y": 10, "width": 100, "height": 80}}],
     "settings": {"features": ["Contrast", "Entropy"], "grayLevels": 32,
       "quantization": {"method": "fixedRange", "min": 0, "max": 255, "binWidth": 8},
       "distances": [1], "directions": [0, 45, 90, 135], "aggregation": "perDirectionAndMean", "logBase": "natural",
       "score": {"enabled": false, "age": 40, "coefficients": [1.138, -1.814, 1.416, 1.714],
                 "profile": "calibration", "intensityMin": 0, "intensityMax": 255}}
   }' | jq -r .analysisId)

   curl -sN -H "$AUTH" $API/analyses/$ANALYSIS/events        # waits until the analysis has finished
   curl -s -H "$AUTH" -OJ $API/analyses/$ANALYSIS/results.csv

``scripts/smoke-test.mjs`` performs the same steps in JavaScript and checks the responses.

.. _api-addon:

Node.js addon
-------------

``@glcm/native`` (``bindings/node/index.d.ts``) is used by the server; it can also be used by other Node.js programs.
Pixel buffers are row-major grayscale samples with 16-bit samples little-endian. Promises reject, and synchronous
functions throw, with an ``Error`` whose ``code`` is ``INVALID_ARGUMENT``, ``UNSUPPORTED_IMAGE``, ``IMAGE_TOO_LARGE``,
``DECODE_FAILED`` or ``INTERNAL_ERROR``; wrong argument types throw a ``TypeError``.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Function
     - Description
   * - ``coreVersion(): string``
     - Version of ``glcm_core``
   * - ``catalog(): NativeCatalog``
     - Features, presets and limits
   * - ``decodeImageFile(path, {maxPixels}?): Promise<DecodedImage>``
     - Size, bit depth, channels, warnings, default window, histogram and pixels of an image file. With ``maxPixels``,
       the size is read from the header first: larger images reject with ``IMAGE_TOO_LARGE`` before decoding, and
       files that are not PNG, JPEG, BMP or TIFF with ``DECODE_FAILED``
   * - ``renderDisplay(pixels, width, height, bitDepth, min, max, maxSize): Promise<Buffer>``
     - PNG with window/level, downscaled to ``maxSize``
   * - ``roiStats(pixels, width, height, bitDepth, roisJson): Promise<NativeRoiStatistics[]>``
     - Pixel count, bounding box and intensity statistics per ROI
   * - ``selectThresholdRegions(pixels, width, height, bitDepth, min, max, minPixels, maxRegions): Promise<{regions, total}>``
     - ``glcm::SelectThresholdRegions``; each region is ``{points, pixelCount, boundingBox}``
   * - ``selectWandRegion(pixels, width, height, bitDepth, x, y, tolerance): Promise<region | null>``
     - ``glcm::SelectWandRegion``
   * - ``validateAnalysis(roisJson, settingsJson): void``
     - Parses and validates an analysis request
   * - ``runAnalysis(pixels, width, height, bitDepth, roisJson, settingsJson): Promise<string>``
     - Every ROI at every distance, as ``glcm-results`` JSON
   * - ``formatResults(resultsJson, format): string``
     - A ``glcm-results`` document written again as ``"csv"`` or canonical ``"json"``
   * - ``exportRoiImages(pixels, width, height, bitDepth, roisJson, settingsJson, transparentOutside, includeQuantized): Promise<ExportedFile[]>``
     - ROI crops, masks, quantized images and manifest as ``{name, data}`` files
   * - ``windowLevel(value, min, max): number``
     - The 8-bit display value of one intensity
   * - ``featureMapGrid(settingsJson, width, height): {step, columns, rows, workPerRow}``
     - Parse and validate feature map settings for an image size; throws ``INVALID_ARGUMENT``. ``workPerRow`` is the
       estimated computing work of one row (``glcm::FeatureMapRowWork``)
   * - ``computeFeatureMap(pixels, width, height, bitDepth, settingsJson, firstRow, rowCount, cancelToken?): Promise<Float32Array>``
     - ``rowCount × columns`` values of rows of a feature map (``glcm::ComputeFeatureMapRows``); rejects with
       ``CANCELLED`` once ``cancelToken`` is cancelled
   * - ``new CancelToken()``, ``token.cancel()``, ``token.cancelled``
     - Stops the ``computeFeatureMap`` calls that received the token after their current point

.. code-block:: javascript

   import * as native from '@glcm/native';

   const image = await native.decodeImageFile('samples/textures/brick.png');
   const rois = [{ id: 'r1', name: 'ROI 1', shape: { type: 'ellipse', cx: 256, cy: 256, rx: 40, ry: 25, angle: 30 } }];
   const settings = { features: ['Contrast', 'Entropy'], grayLevels: 32 };
   const document = JSON.parse(
     await native.runAnalysis(image.pixels, image.width, image.height, image.bitDepth, JSON.stringify(rois), JSON.stringify(settings)),
   );
   console.log(document.results[0].values.Contrast.mean);

Settings objects passed to the addon may omit every field except ``features``; missing fields take the defaults of
``glcm::AnalysisSettings``. The HTTP API requires all fields.

C++ library
-----------

Link against ``glcm_core`` (``add_subdirectory(core)`` and ``target_link_libraries(app PRIVATE glcm_core)``); include
paths are relative to ``core/``. The main entry points:

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Header
     - Functions and types
   * - ``imaging/ImageLoader.hpp``
     - ``LoadImageFile``, ``LoadImageBytes`` → ``LoadedImage{gray, info, warnings}``
   * - ``roi/Roi.hpp``
     - ``RectangleRoi``, ``EllipseRoi``, ``PolygonRoi``, ``Roi``; ``RasterizeMask``, ``RasterizeCroppedMask``, ``MaskBoundingBox``,
       ``CountMaskPixels``
   * - ``roi/RegionSelection.hpp``
     - ``SelectThresholdRegions`` and ``SelectWandRegion``: connected regions of pixel values, with holes filled, as
       polygon outlines along the pixel edges
   * - ``imaging/ImageHeader.hpp``
     - ``ReadImageSize``, ``ReadImageSizeFromBytes`` → ``ImageSize{width, height, more_images, pixel_spacing}``;
       ``PixelSpacing``, ``SpacingFromDensity``
   * - ``imaging/Quantizer.hpp``
     - ``QuantizationSettings``, ``Quantize``
   * - ``analysis/TextureAnalysis.hpp``
     - ``TextureAnalysis``, ``Type``, ``Direction``, ``Features``, ``TextureOptions``
   * - ``analysis/FirstOrder.hpp``
     - ``ComputeFirstOrderStatistics``, ``IsFirstOrderStatistic``
   * - ``analysis/RunLength.hpp``
     - ``ComputeRunLengthMatrix`` → ``RunLengthMatrix``, ``ComputeRunLengthFeatures``, ``IsRunLengthFeature``
   * - ``analysis/SizeZone.hpp``
     - ``ComputeSizeZoneMatrix`` → ``SizeZoneMatrix``, ``ComputeSizeZoneFeatures``, ``IsSizeZoneFeature``
   * - ``analysis/GrayToneDifference.hpp``
     - ``ComputeGrayToneDifferenceMatrix`` → ``GrayToneDifferenceMatrix``, ``ComputeGrayToneDifferenceFeatures``,
       ``IsGrayToneDifferenceFeature``
   * - ``analysis/LocalBinaryPattern.hpp``
     - ``LocalBinaryPatternCode``, ``ComputeLocalBinaryPatternHistogram``, ``ComputeLocalBinaryPatternFeatures``,
       ``IsLocalBinaryPatternFeature``
   * - ``pipeline/AnalysisSettings.hpp``
     - ``AnalysisSettings``, ``DefaultSettings``, ``ValidateSettings``
   * - ``pipeline/AnalysisRunner.hpp``
     - ``RunAnalysis`` → ``AnalysisOutput{results, cancelled}``, computing every feature family of the settings;
       ``ComputeRegionStatistics``
   * - ``pipeline/FeatureCatalog.hpp``
     - ``FeatureCatalog``, ``FeaturePresets``, ``FeatureTypeFromId``
   * - ``pipeline/FeatureMap.hpp``
     - ``FeatureMapSettings``, ``ResolveFeatureMapGrid``, ``ValidateFeatureMapSettings`` and ``ComputeFeatureMapRows``:
       a co-occurrence feature in a sliding window over the whole image
   * - ``imaging/DisplayRenderer.hpp``
     - ``ComputeDisplayStatistics``, ``WindowLevel``, ``RenderWindowLevel``
   * - ``io/Json.hpp``
     - ``RoiSetToJson``/``RoiSetFromJson``, ``SettingsToJson``/``SettingsFromJson``, ``ResultsToJson``/``ResultsFromJson``
   * - ``io/ResultsCsv.hpp``, ``io/RoiImageExport.hpp``
     - ``ResultsToCsv``; ``ExportRoiImages``, ``SanitizeFileName``

.. code-block:: cpp

   #include "imaging/ImageLoader.hpp"
   #include "io/ResultsCsv.hpp"
   #include "pipeline/AnalysisRunner.hpp"

   glcm::LoadedImage image = glcm::LoadImageFile("samples/textures/camera.png");

   glcm::AnalysisSettings settings = glcm::DefaultSettings(image.info.bit_depth); // Haralick F1–F14, Ng = 32
   settings.distances = {1, 2};

   glcm::Roi roi;
   roi.name = "ROI 1";
   roi.shape = glcm::EllipseRoi{256.0, 256.0, 40.0, 25.0, 30.0}; // cx, cy, rx, ry, angle in degrees

   glcm::AnalysisOutput output = glcm::RunAnalysis(image.gray, {roi}, settings);
   double contrast_0_deg = output.results[0].values.at(glcm::Type::Contrast).H;
   // Image name, SHA-256, timestamp and pixel spacing (std::nullopt: no areas in mm²)
   std::string csv = glcm::ResultsToCsv(output.results, settings, {"camera.png", "", "2026-09-14T12:00:00Z", std::nullopt});

Functions throw ``std::invalid_argument`` for invalid input (for example invalid settings); ``RunAnalysis`` reports
problems with a single ROI as a ``Skipped`` or ``Failed`` result instead.

File formats
------------

Every JSON file has ``format`` and an integer ``version``; readers reject other formats and unknown versions.

.. list-table::
   :header-rows: 1
   :widths: 25 20 55

   * - Format
     - File
     - Contents
   * - ``glcm-roi-set``
     - ``*.roi.json``
     - ``image`` (name, width, height, bitDepth, sha256) and ``rois``; written by the web app and by
       ``glcm::RoiSetToJson``
   * - ``glcm-results``
     - ``*-results.json``
     - ``coreVersion``, ``timestamp``, ``image`` (name, sha256), ``settings`` and ``results``
       (``MeasurementResult`` objects)
   * - ``glcm-results-csv``
     - ``*-results.csv``
     - ``# key=value`` lines with the format, versions, image and settings, then a header row and one row per ROI ×
       distance × direction (or per aggregation). Non-standard feature columns end with ``[non-standard]``; numbers
       use the shortest text that reads back to the same double; fields are quoted per RFC 4180; text fields starting
       with ``=``, ``+``, ``-``, ``@``, tab or carriage return get a leading ``'`` (CSV injection)
   * - ``glcm-roi-images``
     - ``manifest.json`` in the ROI images ZIP
     - One entry per ROI with its geometry, bounding box, pixel count and file names, or why it was skipped
   * - ``glcm-project``
     - ``*.glcmproj``
     - ``image`` (name, size, bit depth, sha256, optional base64 ``data``), ``rois`` (with visibility), ``settings``
       and ``results`` (finished analyses with their settings)

Example ROI set:

.. code-block:: json

   {
     "format": "glcm-roi-set",
     "version": 1,
     "image": {"name": "mri16.tif", "width": 512, "height": 512, "bitDepth": 16, "sha256": "…"},
     "rois": [
       {"id": "7f3c", "name": "ROI 1", "color": "#FFD400",
        "shape": {"type": "rectangle", "x": 100, "y": 100, "width": 64, "height": 64}}
     ]
   }
