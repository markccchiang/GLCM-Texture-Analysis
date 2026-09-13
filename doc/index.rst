GLCM Texture Analysis
=====================

GLCM Texture Analysis is an interactive C++ tool that computes Haralick texture features from the Gray Level
Co-occurrence Matrix (GLCM) of a rectangle or polygon selected in a grayscale image.

This documentation describes exactly how ``glcm::TextureAnalysis`` (``analysis/TextureAnalysis.cpp``) builds the
co-occurrence matrices and computes each feature, and lists the literature the features come from.

For building and running the application, see ``README.md`` in the repository root.

.. toctree::
   :maxdepth: 2
   :caption: Contents

   equations
   references

Building this documentation
---------------------------

.. code-block:: bash

   cd doc
   python3 -m venv .venv
   source .venv/bin/activate
   pip install -r requirements.txt
   make html

The HTML pages are written to ``doc/_build/html``; open ``doc/_build/html/index.html`` in a browser.
