GLCM Texture Analysis
=====================

GLCM Texture Analysis computes Haralick texture features from the Gray Level Co-occurrence Matrix (GLCM) of regions of
interest (ROIs) in 8- and 16-bit grayscale images. Users open an image in the browser, draw rectangle, ellipse, polygon
or freehand ROIs, choose the features and GLCM settings, measure, and export the results. The application runs on a
single computer (local mode) or on a server shared by several users (server mode).

This documentation has three parts:

- **User guide** explains how to use the application: opening and viewing images, drawing ROIs, measuring, and saving,
  importing and exporting.
- **Texture features** describes exactly how ``glcm::TextureAnalysis`` (``core/analysis/TextureAnalysis.cpp``) builds
  the co-occurrence matrices and computes each feature, and lists the literature the features come from.
- **Developer guide** describes the architecture of the application, its APIs (HTTP, Node.js addon and C++) and file
  formats, and the technologies and packages it is built with.

For installing and running the application, see ``README.md`` in the repository root; for server deployment, see
``doc/deployment.md``.

.. toctree::
   :maxdepth: 2
   :caption: User guide

   user/index
   user/getting-started
   user/viewing
   user/rois
   user/measuring
   user/files
   user/reference

.. toctree::
   :maxdepth: 2
   :caption: Texture features

   equations
   references

.. toctree::
   :maxdepth: 2
   :caption: Developer guide

   developer/index
   developer/architecture
   developer/api
   developer/technologies

Building this documentation
---------------------------

.. code-block:: bash

   cd doc
   python3 -m venv .venv
   source .venv/bin/activate
   pip install -r requirements.txt
   make html

The HTML pages are written to ``doc/_build/html``; open ``doc/_build/html/index.html`` in a browser.
