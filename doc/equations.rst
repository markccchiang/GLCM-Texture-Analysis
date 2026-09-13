GLCM Texture Equations
======================

This page lists the equations exactly as they are implemented in ``analysis/TextureAnalysis.cpp``. Where the
implementation differs from the usual literature definition, the difference is noted.

Every feature is computed separately for each of the four directions and returned as a ``glcm::Features`` value
with the fields ``H``, ``V``, ``LD`` and ``RD``. ``Features::Avg()`` is the mean of the four directions.

.. contents:: On this page
   :local:
   :depth: 2

Co-occurrence matrix
--------------------

Region and directions
~~~~~~~~~~~~~~~~~~~~~

Let :math:`I` be an 8-bit grayscale image, :math:`\Omega` the selected region, and :math:`d \ge 1` the neighborhood
distance. In rectangle mode :math:`\Omega` is the whole cropped image; in polygon mode it is the set of pixels whose
mask value is 255.

Each direction :math:`\theta` has two neighbor offsets (row, column):

.. list-table::
   :header-rows: 1
   :widths: 15 15 40

   * - Direction
     - Name in code
     - Offsets :math:`(\Delta r, \Delta c)`
   * - 0°
     - ``H``
     - :math:`(0, d)` and :math:`(0, -d)`
   * - 90°
     - ``V``
     - :math:`(d, 0)` and :math:`(-d, 0)`
   * - 135°
     - ``LD``
     - :math:`(d, d)` and :math:`(-d, -d)`
   * - 45°
     - ``RD``
     - :math:`(d, -d)` and :math:`(-d, d)`

Gray levels are **zero-based**: :math:`i, j \in \{0, 1, \dots, N_g - 1\}`, where :math:`N_g` is the number of gray
levels (256 in the application). Haralick's paper [Haralick1973]_ numbers gray levels from 1, so features that
depend on the gray level values themselves (means, correlations, sum average, cluster shade and prominence,
auto correlation) are shifted compared with a one-based implementation.

Counting pixel pairs
~~~~~~~~~~~~~~~~~~~~

For a central pixel :math:`(m, n)` and a neighbor :math:`(k, l) = (m + \Delta r, n + \Delta c)`, the pair is counted
only if **both** pixels are inside the image and inside :math:`\Omega`:

.. math::

   P_\theta(i, j) = \#\left\{ \big((m, n), (k, l)\big) \;:\;
      (m, n) \in \Omega,\ (k, l) \in \Omega,\ (\Delta r, \Delta c) \in \text{offsets}(\theta),\
      I(k, l) = i,\ I(m, n) = j \right\}

Because both offsets of a direction are visited from every central pixel, each unordered pixel pair is counted twice,
once as :math:`(i, j)` and once as :math:`(j, i)`, so :math:`P_\theta` is symmetric.

The normalization factor and the normalized matrix are

.. math::

   R_\theta = \sum_{i=0}^{N_g-1} \sum_{j=0}^{N_g-1} P_\theta(i, j), \qquad
   p(i, j) = \begin{cases}
      P_\theta(i, j) / R_\theta & R_\theta > 0 \\
      0 & R_\theta = 0
   \end{cases}

A direction without any pixel pair (for example 90° in a one-row region) therefore gives an all-zero matrix instead of
NaN values.

In the rest of this page :math:`p(i, j)` denotes the normalized matrix of one direction, and :math:`\sum_{i,j}` means
:math:`\sum_{i=0}^{N_g-1} \sum_{j=0}^{N_g-1}`.

Worked example
~~~~~~~~~~~~~~

For the 4 × 4 image with 4 gray levels from [Haralick1973]_

.. code-block:: text

   0 0 1 1
   0 0 1 1
   0 2 2 2
   2 2 3 3

the 0° matrix at distance 1 is

.. math::

   P_{0^\circ} = \begin{pmatrix}
      4 & 2 & 1 & 0 \\
      2 & 4 & 0 & 0 \\
      1 & 0 & 6 & 1 \\
      0 & 0 & 1 & 2
   \end{pmatrix}, \qquad R_{0^\circ} = 24

This example is checked by the unit test ``TextureAnalysisTest.HaralickExampleHorizontal``.

Marginal probabilities and statistics
-------------------------------------

These are computed once per direction in ``TextureAnalysis::Normalize()``.

.. math::

   p_x(i) = \sum_{j=0}^{N_g-1} p(i, j), \qquad
   p_y(j) = \sum_{i=0}^{N_g-1} p(i, j)

.. math::

   p_{x+y}(k) = \sum_{\substack{i,j \\ i + j = k}} p(i, j), \quad k = 0, \dots, 2N_g - 2
   \qquad
   p_{x-y}(k) = \sum_{\substack{i,j \\ |i - j| = k}} p(i, j), \quad k = 0, \dots, N_g - 1

Means and standard deviations from the marginal vectors:

.. math::

   \mu_x = \sum_{i} i \, p_x(i), \quad
   \mu_y = \sum_{j} j \, p_y(j), \quad
   \sigma_x = \sqrt{\sum_{i} (i - \mu_x)^2 \, p_x(i)}, \quad
   \sigma_y = \sqrt{\sum_{j} (j - \mu_y)^2 \, p_y(j)}

The same statistics summed directly over :math:`p(i, j)`. They are mathematically equal to the ones above and are used
only by the "another way" cross-check features:

.. math::

   \mu_i = \sum_{i,j} i \, p(i, j), \quad
   \mu_j = \sum_{i,j} j \, p(i, j), \quad
   \sigma_i = \sqrt{\sum_{i,j} (i - \mu_i)^2 \, p(i, j)}, \quad
   \sigma_j = \sqrt{\sum_{i,j} (j - \mu_j)^2 \, p(i, j)}

Entropies are computed with the natural logarithm, and terms whose probability is zero are skipped
(:math:`0 \log 0 = 0`).

Features
--------

The first column is the ``glcm::Type`` value passed to ``TextureAnalysis::Calculate()``.

Region statistics
~~~~~~~~~~~~~~~~~

These do not use the co-occurrence matrix. The same value is reported for all four directions. Let
:math:`v_1, \dots, v_N` be the gray levels of the :math:`N` pixels in :math:`\Omega`.

``Mean``
   .. math:: \bar{v} = \frac{1}{N} \sum_{t=1}^{N} v_t

   NaN if the region is empty.

``Std``
   Sample standard deviation:

   .. math:: s = \sqrt{\frac{1}{N - 1} \sum_{t=1}^{N} (v_t - \bar{v})^2}

   0 for a single pixel and NaN for an empty region.

Haralick features
~~~~~~~~~~~~~~~~~

Features F1–F14 of [Haralick1973]_ (see also [Haralick1979]_).

``Energy`` — Angular Second Moment
   .. math:: f = \sum_{i,j} p(i, j)^2

``Contrast``
   .. math:: f = \sum_{n=0}^{N_g-1} n^2 \, p_{x-y}(n)

``ContrastAnotherWay`` — cross-check of ``Contrast``
   .. math:: f = \sum_{i,j} (i - j)^2 \, p(i, j)

``CorrelationII`` — Haralick's correlation
   .. math:: f = \frac{\sum_{i,j} i \, j \, p(i, j) - \mu_x \mu_y}{\sigma_x \sigma_y}

   If :math:`\sigma_x \sigma_y = 0` (for example a constant region), the correlation is undefined and 1 is returned,
   as in PyRadiomics. The same applies to ``CorrelationI``, ``CorrelationIII`` and the "another way" variants (with
   :math:`\sigma_i \sigma_j`).

``CorrelationIIAnotherWay`` — cross-check of ``CorrelationII``
   .. math:: f = \frac{\sum_{i,j} i \, j \, p(i, j) - \mu_i \mu_j}{\sigma_i \sigma_j}

``SumOfSquares`` — variance in :math:`i` and :math:`j`
   .. math:: f = \sum_{i,j} \left[ (i - \mu_i)^2 + (j - \mu_j)^2 \right] p(i, j)

   Haralick's F4 "Sum of Squares: Variance" uses only one of the two terms; see ``SumOfSquaresI``.

``SumOfSquaresI`` — variance in :math:`i`
   .. math:: f = \sum_{i,j} (i - \mu_i)^2 \, p(i, j)

``SumOfSquaresJ`` — variance in :math:`j`
   .. math:: f = \sum_{i,j} (j - \mu_j)^2 \, p(i, j)

``HomogeneityII`` — Inverse Difference Moment
   .. math:: f = \sum_{i,j} \frac{p(i, j)}{1 + (i - j)^2}

``SumAverage``
   .. math:: f_{SA} = \sum_{k=0}^{2N_g-2} k \, p_{x+y}(k)

``SumVariance``
   .. math:: f = \sum_{k=0}^{2N_g-2} \left(k - f_{SA}\right)^2 p_{x+y}(k)

   Centered on the Sum Average. The printed paper [Haralick1973]_ centers it on the Sum Entropy, which is a
   known typo.

``SumEntropy``
   .. math:: f = -\sum_{k=0}^{2N_g-2} p_{x+y}(k) \log p_{x+y}(k)

``Entropy``
   .. math:: f = -\sum_{i,j} p(i, j) \log p(i, j)

``DifferenceVariance``
   .. math::

      \mu_{x-y} = \sum_{k=0}^{N_g-1} k \, p_{x-y}(k), \qquad
      f = \sum_{k=0}^{N_g-1} \left(k - \mu_{x-y}\right)^2 p_{x-y}(k)

``DifferenceEntropy``
   .. math:: f = -\sum_{k=0}^{N_g-1} p_{x-y}(k) \log p_{x-y}(k)

``InformationMeasuresOfCorrelationI`` and ``InformationMeasuresOfCorrelationII``
   With the entropies

   .. math::

      HX  &= -\sum_{i} p_x(i) \log p_x(i), \qquad
      HY   = -\sum_{j} p_y(j) \log p_y(j), \qquad
      HXY  = -\sum_{i,j} p(i, j) \log p(i, j) \\
      HXY1 &= -\sum_{i,j} p(i, j) \log\big(p_x(i) \, p_y(j)\big), \qquad
      HXY2  = -\sum_{i,j} p_x(i) \, p_y(j) \log\big(p_x(i) \, p_y(j)\big)

   where terms with :math:`p_x(i) \, p_y(j) = 0` are skipped,

   .. math::

      f_{IMC1} = \frac{HXY - HXY1}{\max(HX, HY)}, \qquad
      f_{IMC2} = \sqrt{1 - \exp\big(-2 \, (HXY2 - HXY)\big)}

   If :math:`\max(HX, HY) = 0` (a single gray level), :math:`f_{IMC1} = 0`. If rounding makes
   :math:`1 - \exp(-2 \, (HXY2 - HXY))` negative, :math:`f_{IMC2} = 0`. These are the values PyRadiomics uses.

   Requesting either type calculates both.

Maximal Correlation Coefficient
   Available through ``TextureAnalysis::GetMaximalCorrelationCoefficient()`` only; it is not a ``glcm::Type``.

   .. math:: Q(i, j) = \sum_{k=0}^{N_g-1} \frac{p(i, k) \, p(j, k)}{p_x(i) \, p_y(k)}

   where terms with :math:`p_x(i) \, p_y(k) = 0` are skipped. With :math:`\lambda_2` the second largest real part of
   the eigenvalues of :math:`Q` (computed with Eigen),

   .. math:: f = \sqrt{\max(\lambda_2, 0)}

   A negative :math:`\lambda_2` can only come from rounding and is treated as 0.

Other co-occurrence features
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``AutoCorrelation``
   .. math:: f = \sum_{i,j} i \, j \, p(i, j)

``CorrelationI``
   .. math:: f = \sum_{i,j} \frac{(i - \mu_x)(j - \mu_y) \, p(i, j)}{\sigma_x \sigma_y}

   Mathematically equal to ``CorrelationII``.

``CorrelationIAnotherWay`` — cross-check of ``CorrelationI``
   .. math:: f = \sum_{i,j} \frac{(i - \mu_i)(j - \mu_j) \, p(i, j)}{\sigma_i \sigma_j}

``CorrelationIII``
   .. math:: f = \frac{\sum_{i,j} i \, j \, p(i, j) - \mu_x \mu_y}{\sigma_x^2 \, \sigma_y^2}

   The source code attributes this form to a paper by Xiaofeng Yang, probably [Yang2012]_.

``ClusterShade``
   .. math:: f = \sum_{i,j} (i + j - \mu_x - \mu_y)^3 \, p(i, j)

   Commonly cited from [Conners1984]_.

``ClusterProminence``
   .. math:: f = \sum_{i,j} (i + j - \mu_x - \mu_y)^4 \, p(i, j)

   Commonly cited from [Conners1984]_.

``Dissimilarity``
   .. math:: f = \sum_{i,j} |i - j| \, p(i, j)

``HomogeneityI``
   .. math:: f = \sum_{i,j} \frac{p(i, j)}{1 + |i - j|}

``MaximumProbability``
   .. math:: f = \max_{i,j} \, p(i, j)

``InverseDifferenceNormalized``
   .. math:: f = \sum_{i,j} \frac{p(i, j)}{1 + |i - j| / N_g}

   See [Clausi2002]_.

``InverseDifferenceMomentNormalized``
   .. math:: f = \sum_{i,j} \frac{p(i, j)}{1 + (i - j)^2 / N_g^2}

   See [Clausi2002]_.

Auto Correlation, Dissimilarity and Maximum Probability are often cited from [Soh1999]_.

Score
~~~~~

``TextureAnalysis::CalculateScore(age, features)`` adds ``Score`` and ``Age`` when ``Mean``, ``Entropy`` and
``Contrast`` are present and :math:`\text{age} > 0`. For each direction:

.. math::

   \text{Score} = 1.138 \cdot \text{age} - 1.814 \cdot \text{Mean} + 1.416 \cdot \text{Entropy}
                  + 1.714 \cdot \text{Contrast}

``Age`` stores the age value in all four directions.

Choosing features and gray levels
---------------------------------

- The value of most features depends on :math:`N_g` and on how the image is quantized, see [Clausi2002]_,
  [Soh1999]_, [Brynolfsson2017]_ and [Lofstedt2019]_.
- Standardized feature definitions and reference values are given by the Image Biomarker Standardization
  Initiative [Zwanenburg2020]_ and implemented in PyRadiomics [vanGriethuysen2017]_.
- [HallBeyer2017a]_ and [HallBeyer2017b]_ give practical guidance on interpreting and selecting GLCM features.
- The ImageJ plugin kept in ``ImageJ-plugin-codes/`` computes its correlation according to [Walker1995]_.
