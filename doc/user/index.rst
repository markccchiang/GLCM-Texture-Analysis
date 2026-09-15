.. _user-guide:

User guide
==========

GLCM Texture Analysis measures the texture of regions in grayscale images. A typical session has five steps:

#. **Open an image** — PNG, JPEG, BMP or TIFF, 8 or 16 bits per pixel (:ref:`getting-started`).
#. **Look at it** — zoom, pan and adjust the display window to see the structures you want to measure
   (:ref:`viewing`).
#. **Draw regions of interest (ROIs)** — rectangles, ellipses, polygons or freehand outlines, collected in the ROI
   Manager (:ref:`rois`).
#. **Measure** — choose the texture features and the analysis settings, and measure the selected or all ROIs, or the
   same ROIs on a batch of images; the results appear in a table and as plots (:ref:`measuring`).
#. **Save and export** — export the results as CSV or JSON, save the ROIs for another session, export the ROI images,
   or save everything as a project (:ref:`files`).

.. figure:: images/main-window.png
   :alt: The main window with the sample image, four ROIs, the ROI Manager, the analysis settings and the results table.
   :width: 100%

   The main window after measuring four ROIs on the sample image.

The application runs in a web browser (a current version of Chrome, Edge, Firefox or Safari). It can run on your own
computer, or on a server that several people use; the guide points out where the two differ.

The texture features and the settings that control them are defined precisely in :doc:`../equations`.

.. toctree::
   :maxdepth: 2

   getting-started
   viewing
   rois
   measuring
   files
   reference
