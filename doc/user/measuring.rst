.. _measuring:

Measuring
=========

Analysis settings
-----------------

The **Analysis Settings** panel decides what the next measurement computes. The settings are remembered between
sessions, and results already in the table keep the settings they were measured with.

.. figure:: images/analysis-settings.png
   :alt: The Analysis Settings panel with preset, features, gray levels, quantization, distances, directions and
         aggregation.
   :align: center

   The Analysis Settings panel.

.. list-table::
   :header-rows: 1
   :widths: 22 78

   * - Setting
     - Meaning
   * - **Preset**
     - A named set of features: *Haralick F1–F14*, *Clausi (2002): Contrast, Correlation, Entropy*, *Basic*,
       *Score (Mean, Entropy, Contrast)* (the inputs of the age-based score, with the score switched on) or *All
       features*. Changing the features by hand shows *Custom*.
   * - **Features**
     - The features to compute. **N selected…** opens the feature picker.
   * - **Gray levels (Ng)**
     - How many gray levels the intensities are reduced to before the co-occurrence matrix is built: 2–256; the menu
       in the field offers 8, 16, 32, 64, 128 and 256. More levels keep more detail but need larger regions for
       stable values.
   * - **Quantization**
     - How intensities are mapped to gray levels: **Fixed range** (from *Min* to *Max*, the same for every ROI —
       usually the full range of the image), **ROI min–max** (from the lowest to the highest intensity inside each
       ROI), **Fixed bin width** (a fixed number of intensities per gray level) or **None** (intensities are used
       directly and must be below Ng).
   * - **Distances**
     - The distances in pixels between the two pixels of a pair, e.g. ``1, 2, 4``. Each distance is measured
       separately.
   * - **Directions**
     - The directions of pixel pairs: 0° (horizontal), 45°, 90° (vertical) and 135°.
   * - **Aggregation**
     - Which rows the results show per ROI and distance: **Per direction + mean**, **Mean only**, or **Mean + range**
       (mean and range over the directions, as proposed by Haralick in 1973).
   * - **Log base** (Advanced)
     - Natural logarithm, or log₂ to compare entropies with tools such as PyRadiomics or mahotas.
   * - **Age-based score** (Advanced)
     - Adds a *Score* column computed from the age, mean, entropy and contrast with the given coefficients.

Problems with the settings are listed at the bottom of the panel: errors in red prevent measuring, warnings in yellow
point out, for example, that the Maximal Correlation Coefficient is slow for more than 64 gray levels. **Reset to
defaults** restores the default settings (Haralick features, 32 gray levels, full fixed range, distance 1, all
directions).

.. figure:: images/feature-picker.png
   :alt: The feature picker with a search field and checkboxes grouped into region statistics, Haralick and other
         features; Correlation III and Sum of Squares are marked non-standard.
   :align: center

   The feature picker.

In the **feature picker**, features are grouped as in :doc:`../equations`. Type in the search field to filter them;
**Select all** and **Clear all** apply to the listed features. Two features are marked **⚠ non-standard**
(*Correlation III* and *Sum of Squares (in x and y)*): they follow a formula as printed in a later paper rather than
the original definition; hover the badge for details. Features marked **slow** take noticeably longer for many gray
levels.

.. rubric:: The age-based score

The score combines the age with the mean intensity, entropy and contrast using four coefficients. Its coefficients were
fitted for 8-bit images with 256 gray levels, distance 1 and all four directions. With the **Calibration** profile
(default) its inputs are always computed that way, whatever the other settings are; 16-bit images are first mapped to
0–255 using the display window. With the **Current settings** profile the inputs use the panel's settings, and the
results carry a warning that the coefficients may not apply. **Reset age and coefficients** restores the defaults.

Measuring ROIs
--------------

- **Measure** in the toolbar, :kbd:`M` or *Analyze ▸ Measure Selected* measures the selected ROIs (or the active ROI, if
  none is selected).
- The arrow next to **Measure**, :kbd:`Shift+M` or *Analyze ▸ Measure All* measures every ROI in the ROI Manager.

Each ROI is measured at each distance as a separate job. While jobs run, the status bar shows their progress, for
example *3/8 jobs*; the **×** next to it cancels the measurement (jobs already started still finish, and their results
are kept). You can keep working — drawing ROIs or changing the settings does not affect a running measurement.

Results
-------

.. figure:: images/results-table.png
   :alt: The Results table with columns ROI, d, Dir, Pixels, Ng, the features, Score and Status.
   :width: 100%

   The Results table.

Every measurement adds rows to the **Results** table:

- One row per **ROI**, **distance** (``d``) and **direction** (``Dir``: 0°, 45°, 90°, 135°, Mean or Range, depending
  on the aggregation), with the **pixel count**, the **gray levels** and one column per feature. The **Score** column
  appears when a measurement included the score.
- Values are shown with six significant digits; copied and exported values have full precision.
- **Status** is empty for normal results, **⚠** when the result has warnings, or *skipped* / *failed* with the reason —
  for example an ROI with fewer than 2 pixels, or intensities above the gray levels when quantization is *None*.
- Hover a row to see the image and settings it was measured with, and its warnings. Hovering also highlights the ROI
  on the canvas; clicking selects it.

Use the buttons above the table to work with it:

- Click a **column header** to sort by it; click again to reverse the order, and a third time to restore the order of
  measurement.
- **Columns** shows or hides columns.
- **Copy** copies the table (visible columns, current order) as tab-separated text, ready to paste into a spreadsheet.
- **Export** saves the results as CSV or JSON (see :ref:`export-results`).
- The trash button clears the table (*Analyze ▸ Clear Results*).

.. note::

   Mean and Std (region statistics) are computed from the original intensities of the ROI. All other features use the
   quantized gray levels, so they depend on the gray levels and quantization settings. The formulas are listed in
   :doc:`../equations`.
