
//=====================================================
//Name:           GLCM_Texture
//Project:         Gray Level Correlation Matrix Texture Analyzer
//Version:         0.4
//
//Author:           Julio E. Cabrera
//Date:             06/10/05
//Comment:       Calculates texture features based in Gray Level Correlation Matrices
//	   Changes since 0.1 version: The normalization constant (R in Haralick's paper, pixelcounter here)
//	   now takes in account the fact that for each pair of pixel you take in account there are two entries to the
//	   grey level co-ocurrence matrix
//		   Changes were made also in the Correlation parameter. Now this parameter is calculated according to Walker's paper
//=====================================================

//===========imports===================================
import ij.*;
import ij.gui.*;
import ij.plugin.filter.PlugInFilter;
import ij.process.*;
import java.awt.*;
import ij.plugin.PlugIn;
import ij.text.*;
import ij.measure.ResultsTable;

//===========source====================================
public class GLCM_Texture7 implements PlugInFilter {
	static int step = 1;
	static String selectedStep = "Average of all angles";
	static boolean doIshowBasic = true;
	static boolean doIcalculateASM = false; //
	static boolean doIcalculateContrast = true; //
	static boolean doIcalculateCorrelation = true; //
	static boolean doIcalculateIDM = false; //
	static boolean doIcalculateEntropy = false; //
	static boolean doIcalculateDissimilarity = false;
	static boolean doIcalculateINV = false;
	static boolean doIcalculateVariance = false; //
	static boolean doIcalculateClusterShade = false; //
	static boolean doIcalculateClusterProminence = false;
	static boolean doIcalculateINN = false;
	static boolean doIcalculateIDN = false;
	static boolean doIcalculateMaxProb = false; //
	static boolean doIcalculateSumAvg = false;
	static boolean doIcalculateSumEnth = false;
	static boolean doIcalculateSumVar = false;
	static boolean doIcalculateDiffVar = false;
	static boolean doIcalculateDiffEnth = false;
	static boolean doCheckCounts = true;

	ResultsTable rt = ResultsTable.getResultsTable();

	public int setup(String arg, ImagePlus imp) {
		if (imp != null && !showDialog())
			return DONE;
		rt.reset();
		return DOES_8G + DOES_STACKS + SUPPORTS_MASKING;
	}

	public void run(ImageProcessor ip) {

		// This part get all the pixel values into the pixel [ ] array via the
		// Image Processor

		ImagePlus imp = IJ.getImage();
		Roi roi = imp.getRoi();
		if (roi != null && !roi.isArea())
			roi = null;
		ip = imp.getProcessor();
		ImageProcessor mask = roi != null ? roi.getMask() : null;
		Rectangle r = roi != null ? roi.getBounds() : new Rectangle(0, 0, ip.getWidth(), ip.getHeight());

		byte[] pixels = (byte[]) ip.getPixels();
		int width = ip.getWidth();
		int height = ip.getHeight();
		// Rectangle r = ip.getRoi();

		// The variable a holds the value of the pixel where the Image Processor
		// is sitting its attention
		// The variable b holds the value of the pixel which is the neighbor to
		// the pixel where the Image Processor is sitting its attention

		int a;
		int b;
		int b1, b2, b3, b4;
		double pixelCounter1 = 0;
		double pixelCounter2 = 0;
		double pixelCounter3 = 0;
		double pixelCounter4 = 0;
		double pixelCounter5 = 0;
		int roi_height = r.height;
		int roi_width = r.width;
		int r_x = r.x;
		int r_y = r.y;
		int pixel_min = 256;
		int pixel_max = -1;

		// for 8-bit figure, set the dimension of grey level is 2^8 = 256
		int Ng = 256;

		// setup the show table
		rt.incrementCounter();
		int row = rt.getCounter() - 1;

		// ====================================================================================================
		// This part computes the Gray Level Correlation Matrix based in the
		// step selected by the user

		int offset, i;
		double[][] glcm1 = new double[Ng][Ng]; // for 0 degrees
		double[][] glcm2 = new double[Ng][Ng]; // for 45 degrees
		double[][] glcm3 = new double[Ng][Ng]; // for 90 degrees
		double[][] glcm4 = new double[Ng][Ng]; // for 135 degrees
		double[][] glcm5 = new double[Ng][Ng]; // for average of 0, 45, 90 and
												// 135 degrees

    // get min/max pixel values
		int tmp_pixel;
		for (int y = 0; y < r.height; y++) {
			for (int x = 0; x < r.width; x++) {
				if (mask == null || mask.getPixel(x, y) != 0) {
					tmp_pixel = (int)ip.getPixelValue(x+r.x, y+r.y);
					if (tmp_pixel < pixel_min) {
						pixel_min = tmp_pixel;
					}
					if (tmp_pixel > pixel_max) {
						pixel_max = tmp_pixel;
					}
				}
			}
		}


		// for 0 degrees
		for (int y = 0; y < r.height; y++) {
			for (int x = 0; x < r.width; x++) {
				if (mask == null || mask.getPixel(x, y) != 0) {
					if (x+step >= 0 && x+step < r.width && y >= 0 && y < r.height) {
						a = (int)ip.getPixelValue(x+r.x, y+r.y);
						b = (int)ip.getPixelValue(x+r.x+step, y+r.y);
						glcm1[a][b] += 1;
						glcm1[b][a] += 1;
						pixelCounter1 += 2;
					}
				}
			}
		}

		// for 45 degrees
		for (int y = 0; y < r.height; y++) {
			for (int x = 0; x < r.width; x++) {
				if (mask == null || mask.getPixel(x, y) != 0) {
					if (x+step >= 0 && x+step < r.width && y+step >= 0 && y+step < r.height) {
						a = (int)ip.getPixelValue(x+r.x, y+r.y);
						b = (int)ip.getPixelValue(x+r.x+step, y+r.y+step);
						glcm2[a][b] += 1;
						glcm2[b][a] += 1;
						pixelCounter2 += 2;
					}
				}
			}
		}

		// for 90 degrees
		for (int y = 0; y < r.height; y++) {
			for (int x = 0; x < r.width; x++) {
				if (mask == null || mask.getPixel(x, y) != 0) {
					if (x >= 0 && x < r.width && y+step >=0 && y+step < r.height) {
						a = (int)ip.getPixelValue(x+r.x, y+r.y);
						b = (int)ip.getPixelValue(x+r.x, y+r.y+step);
						glcm3[a][b] += 1;
						glcm3[b][a] += 1;
						pixelCounter3 += 2;
					}
				}
			}
		}

		// for 135 degrees
		for (int y = 0; y < r.height; y++) {
			for (int x = 0; x < r.width; x++) {
				if (mask == null || mask.getPixel(x, y) != 0) {
					if (x-step >= 0 && x-step < r.width && y+step >=0 && y+step < r.height) {
						a = (int)ip.getPixelValue(x+r.x, y+r.y);
						b = (int)ip.getPixelValue(x+r.x-step, y+r.y+step);
						glcm4[a][b] += 1;
						glcm4[b][a] += 1;
						pixelCounter4 += 2;
					}
				}
			}
		}

		for (int y = 0; y < r.height; y++) {
			for (int x = 0; x < r.width; x++) {
				if (mask == null || mask.getPixel(x, y) != 0) {
					if (x >= 0 && x < r.width && y >=0 && y < r.height) {
						if (x+step >= 0 && x+step < r.width && y >=0 && y < r.height) {
							if (x+step >= 0 && x+step < r.width && y+step >=0 && y+step < r.height) {
								if (x >= 0 && x < r.width && y+step >=0 && y+step < r.height) {
									if (x-step >= 0 && x-step < r.width && y+step >=0 && y+step < r.height) {
										a = (int)ip.getPixelValue(x+r.x, y+r.y);
										b1 = (int)ip.getPixelValue(x+r.x+step, y+r.y); // for 0 degrees
										b2 = (int)ip.getPixelValue(x+r.x+step, y+r.y+step); // for 45 degrees
										b3 = (int)ip.getPixelValue(x+r.x, y+r.y+step); // for 90 degrees
										b4 = (int)ip.getPixelValue(x+r.x-step, y+r.y+step); // for 135 degrees
										glcm5[a][b1] += 1;
										glcm5[b1][a] += 1;
										glcm5[a][b2] += 1;
										glcm5[b2][a] += 1;
										glcm5[a][b3] += 1;
										glcm5[b3][a] += 1;
										glcm5[a][b4] += 1;
										glcm5[b4][a] += 1;
										pixelCounter5 += 8;
									}
								}
							}
						}
					}
				}
			}
		}

		// =====================================================================================================
		// This part divides each member of the glcm matrix by the number of
		// pixels. The number of pixels was stored in the pixelCounter variable
		// The number of pixels is used as a normalizing constant

		for (a = 0; a < Ng; a++) {
			for (b = 0; b < Ng; b++) {
				glcm1[a][b] = (glcm1[a][b]) / (pixelCounter1);
				glcm2[a][b] = (glcm2[a][b]) / (pixelCounter2);
				glcm3[a][b] = (glcm3[a][b]) / (pixelCounter3);
				glcm4[a][b] = (glcm4[a][b]) / (pixelCounter4);
				glcm5[a][b] = (glcm5[a][b]) / (pixelCounter5);
			}
		}

		// =====================================================================================================
		// This part calculate the meanx, meany, stdevx and stdevy for 2-D glcm
		// matrix
		double meanx1 = 0.0, meanx2 = 0.0, meanx3 = 0.0, meanx4 = 0.0, meanx5 = 0.0;
		double meany1 = 0.0, meany2 = 0.0, meany3 = 0.0, meany4 = 0.0, meany5 = 0.0;
		double stdevx1 = 0.0, stdevx2 = 0.0, stdevx3 = 0.0, stdevx4 = 0.0, stdevx5 = 0.0;
		double stdevy1 = 0.0, stdevy2 = 0.0, stdevy3 = 0.0, stdevy4 = 0.0, stdevy5 = 0.0;

		// Calculate the means
		for (a = 0; a < Ng; a++) {
			for (b = 0; b < Ng; b++) {
				meanx1 = meanx1 + a * glcm1[a][b];
				meanx2 = meanx2 + a * glcm2[a][b];
				meanx3 = meanx3 + a * glcm3[a][b];
				meanx4 = meanx4 + a * glcm4[a][b];
				meanx5 = meanx5 + a * glcm5[a][b];

				meany1 = meany1 + b * glcm1[a][b];
				meany2 = meany2 + b * glcm2[a][b];
				meany3 = meany3 + b * glcm3[a][b];
				meany4 = meany4 + b * glcm4[a][b];
				meany5 = meany5 + b * glcm5[a][b];
			}
		}

		// Now calculate the standard deviations
		for (a = 0; a < Ng; a++) {
			for (b = 0; b < Ng; b++) {
				stdevx1 = stdevx1 + (a - meanx1) * (a - meanx1) * glcm1[a][b];
				stdevx2 = stdevx2 + (a - meanx2) * (a - meanx2) * glcm2[a][b];
				stdevx3 = stdevx3 + (a - meanx3) * (a - meanx3) * glcm3[a][b];
				stdevx4 = stdevx4 + (a - meanx4) * (a - meanx4) * glcm4[a][b];
				stdevx5 = stdevx5 + (a - meanx5) * (a - meanx5) * glcm5[a][b];

				stdevy1 = stdevy1 + (b - meany1) * (b - meany1) * glcm1[a][b];
				stdevy2 = stdevy2 + (b - meany2) * (b - meany2) * glcm2[a][b];
				stdevy3 = stdevy3 + (b - meany3) * (b - meany3) * glcm3[a][b];
				stdevy4 = stdevy4 + (b - meany4) * (b - meany4) * glcm4[a][b];
				stdevy5 = stdevy5 + (b - meany5) * (b - meany5) * glcm5[a][b];
			}
		}

		// =====================================================================================================
		// This part calculate p_{x+y} and p_{x-y}
		double[] p_x_plus_y1 = new double[2 * Ng - 1];
		double[] p_x_plus_y2 = new double[2 * Ng - 1];
		double[] p_x_plus_y3 = new double[2 * Ng - 1];
		double[] p_x_plus_y4 = new double[2 * Ng - 1];
		double[] p_x_plus_y5 = new double[2 * Ng - 1];

		double[] p_x_minus_y1 = new double[Ng];
		double[] p_x_minus_y2 = new double[Ng];
		double[] p_x_minus_y3 = new double[Ng];
		double[] p_x_minus_y4 = new double[Ng];
		double[] p_x_minus_y5 = new double[Ng];

		int k, m, n;

		for (k = 2; k <= 2 * Ng; k++) {
			for (m = 1; m <= Ng; m++) {
				n = k - m;
				if (n >= 1 && n <= Ng) {
					p_x_plus_y1[k - 2] = p_x_plus_y1[k - 2] + glcm1[m - 1][n - 1];
					p_x_plus_y2[k - 2] = p_x_plus_y2[k - 2] + glcm2[m - 1][n - 1];
					p_x_plus_y3[k - 2] = p_x_plus_y3[k - 2] + glcm3[m - 1][n - 1];
					p_x_plus_y4[k - 2] = p_x_plus_y4[k - 2] + glcm4[m - 1][n - 1];
					p_x_plus_y5[k - 2] = p_x_plus_y5[k - 2] + glcm5[m - 1][n - 1];
				}
			}
		}

		for (k = 0; k <= Ng - 1; k++) {
			for (m = 1; m <= Ng; m++) {
				for (n = 1; n <= Ng; n++) {
					if (Math.abs(m - n) == k) {
						p_x_minus_y1[k] = p_x_minus_y1[k] + glcm1[m - 1][n - 1];
						p_x_minus_y2[k] = p_x_minus_y2[k] + glcm2[m - 1][n - 1];
						p_x_minus_y3[k] = p_x_minus_y3[k] + glcm3[m - 1][n - 1];
						p_x_minus_y4[k] = p_x_minus_y4[k] + glcm4[m - 1][n - 1];
						p_x_minus_y5[k] = p_x_minus_y5[k] + glcm5[m - 1][n - 1];
					}
				}
			}
		}

		// calculate the mean of p_x_minus_y
		double sum_p_x_minus_y1 = 0.0, sum_p_x_minus_y2 = 0.0, sum_p_x_minus_y3 = 0.0, sum_p_x_minus_y4 = 0.0,
				sum_p_x_minus_y5 = 0.0;
		for (k = 0; k <= Ng - 1; k++) {
			sum_p_x_minus_y1 = sum_p_x_minus_y1 + p_x_minus_y1[k];
			sum_p_x_minus_y2 = sum_p_x_minus_y2 + p_x_minus_y2[k];
			sum_p_x_minus_y3 = sum_p_x_minus_y3 + p_x_minus_y3[k];
			sum_p_x_minus_y4 = sum_p_x_minus_y4 + p_x_minus_y4[k];
			sum_p_x_minus_y5 = sum_p_x_minus_y5 + p_x_minus_y5[k];
		}
		double mean_p_x_minus_y1 = sum_p_x_minus_y1 / Ng;
		double mean_p_x_minus_y2 = sum_p_x_minus_y2 / Ng;
		double mean_p_x_minus_y3 = sum_p_x_minus_y3 / Ng;
		double mean_p_x_minus_y4 = sum_p_x_minus_y4 / Ng;
		double mean_p_x_minus_y5 = sum_p_x_minus_y5 / Ng;

		// ===============================================================================================
		// This part calculates the sum of all GLCM elements
		double sumall1 = 0.0, sumall2 = 0.0, sumall3 = 0.0, sumall4 = 0.0, sumall5 = 0.0;
		for (a = 0; a < Ng; a++) {
			for (b = 0; b < Ng; b++) {
				sumall1 = sumall1 + glcm1[a][b];
				sumall2 = sumall2 + glcm2[a][b];
				sumall3 = sumall3 + glcm3[a][b];
				sumall4 = sumall4 + glcm4[a][b];
				sumall5 = sumall5 + glcm5[a][b];
			}
		}

		rt.setValue("Angle (degree)", row, 0);
		rt.setValue("Angle (degree)", row + 1, 45);
		rt.setValue("Angle (degree)", row + 2, 90);
		rt.setValue("Angle (degree)", row + 3, 135);
		rt.setValue("Angle (degree)", row + 4, "Average of all agnles");

		if (doIshowBasic == true) {
			rt.setValue("Mean-x", row, meanx1);
			rt.setValue("Mean-x", row + 1, meanx2);
			rt.setValue("Mean-x", row + 2, meanx3);
			rt.setValue("Mean-x", row + 3, meanx4);
			rt.setValue("Mean-x", row + 4, meanx5);

			rt.setValue("Mean-y", row, meany1);
			rt.setValue("Mean-y", row + 1, meany2);
			rt.setValue("Mean-y", row + 2, meany3);
			rt.setValue("Mean-y", row + 3, meany4);
			rt.setValue("Mean-y", row + 4, meany5);

			rt.setValue("STD-x", row, stdevx1);
			rt.setValue("STD-x", row + 1, stdevx2);
			rt.setValue("STD-x", row + 2, stdevx3);
			rt.setValue("STD-x", row + 3, stdevx4);
			rt.setValue("STD-x", row + 4, stdevx5);

			rt.setValue("STD-y", row, stdevy1);
			rt.setValue("STD-y", row + 1, stdevy2);
			rt.setValue("STD-y", row + 2, stdevy3);
			rt.setValue("STD-y", row + 3, stdevy4);
			rt.setValue("STD-y", row + 4, stdevy5);

			rt.setValue("Sum of GLCM elements", row, sumall1);
			rt.setValue("Sum of GLCM elements", row + 1, sumall2);
			rt.setValue("Sum of GLCM elements", row + 2, sumall3);
			rt.setValue("Sum of GLCM elements", row + 3, sumall4);
			rt.setValue("Sum of GLCM elements", row + 4, sumall5);

			rt.setValue("ROI Height", row, roi_height);
			rt.setValue("ROI Height", row + 1, roi_height);
			rt.setValue("ROI Height", row + 2, roi_height);
			rt.setValue("ROI Height", row + 3, roi_height);
			rt.setValue("ROI Height", row + 4, roi_height);

			rt.setValue("ROI Width", row, roi_width);
			rt.setValue("ROI Width", row + 1, roi_width);
			rt.setValue("ROI Width", row + 2, roi_width);
			rt.setValue("ROI Width", row + 3, roi_width);
			rt.setValue("ROI Width", row + 4, roi_width);

			rt.setValue("IMG Height", row, height);
			rt.setValue("IMG Height", row + 1, height);
			rt.setValue("IMG Height", row + 2, height);
			rt.setValue("IMG Height", row + 3, height);
			rt.setValue("IMG Height", row + 4, height);

			rt.setValue("IMG Width", row, width);
			rt.setValue("IMG Width", row + 1, width);
			rt.setValue("IMG Width", row + 2, width);
			rt.setValue("IMG Width", row + 3, width);
			rt.setValue("IMG Width", row + 4, width);

			rt.setValue("r.x", row, r_x);
			rt.setValue("r.x", row + 1, r_x);
			rt.setValue("r.x", row + 2, r_x);
			rt.setValue("r.x", row + 3, r_x);
			rt.setValue("r.x", row + 4, r_x);

			rt.setValue("r.y", row, r_y);
			rt.setValue("r.y", row + 1, r_y);
			rt.setValue("r.y", row + 2, r_y);
			rt.setValue("r.y", row + 3, r_y);
			rt.setValue("r.y", row + 4, r_y);

			rt.setValue("pixel min", row, pixel_min);
			rt.setValue("pixel min", row + 1, pixel_min);
			rt.setValue("pixel min", row + 2, pixel_min);
			rt.setValue("pixel min", row + 3, pixel_min);
			rt.setValue("pixel min", row + 4, pixel_min);

			rt.setValue("pixel max", row, pixel_max);
			rt.setValue("pixel max", row + 1, pixel_max);
			rt.setValue("pixel max", row + 2, pixel_max);
			rt.setValue("pixel max", row + 3, pixel_max);
			rt.setValue("pixel max", row + 4, pixel_max);
		}

		// =====================================================================================================
		// This part calculates the angular second moment
		if (doIcalculateASM == true) {
			double asm1 = 0.0, asm2 = 0.0, asm3 = 0.0, asm4 = 0.0, asm5 = 0.0;
			for (a = 0; a < Ng; a++) {
				for (b = 0; b < Ng; b++) {
					asm1 = asm1 + (glcm1[a][b] * glcm1[a][b]);
					asm2 = asm2 + (glcm2[a][b] * glcm2[a][b]);
					asm3 = asm3 + (glcm3[a][b] * glcm3[a][b]);
					asm4 = asm4 + (glcm4[a][b] * glcm4[a][b]);
					asm5 = asm5 + (glcm5[a][b] * glcm5[a][b]);
				}
			}
			rt.setValue("ASM", row, asm1);
			rt.setValue("ASM", row + 1, asm2);
			rt.setValue("ASM", row + 2, asm3);
			rt.setValue("ASM", row + 3, asm4);
			rt.setValue("ASM", row + 4, asm5);
		}

		// =====================================================================================================
		// This part calculates the contrast
		if (doIcalculateContrast == true) {
			double contrast1 = 0.0, contrast2 = 0.0, contrast3 = 0.0, contrast4 = 0.0, contrast5 = 0.0;
			for (a = 0; a < Ng; a++) {
				for (b = 0; b < Ng; b++) {
					contrast1 = contrast1 + (a - b) * (a - b) * (glcm1[a][b]);
					contrast2 = contrast2 + (a - b) * (a - b) * (glcm2[a][b]);
					contrast3 = contrast3 + (a - b) * (a - b) * (glcm3[a][b]);
					contrast4 = contrast4 + (a - b) * (a - b) * (glcm4[a][b]);
					contrast5 = contrast5 + (a - b) * (a - b) * (glcm5[a][b]);
				}
			}
			rt.setValue("Contrast", row, contrast1);
			rt.setValue("Contrast", row + 1, contrast2);
			rt.setValue("Contrast", row + 2, contrast3);
			rt.setValue("Contrast", row + 3, contrast4);
			rt.setValue("Contrast", row + 4, contrast5);
		}

		// =====================================================================================================
		// This part calculates the correlation
		if (doIcalculateCorrelation == true) {
			double correlation1 = 0.0, correlation2 = 0.0, correlation3 = 0.0, correlation4 = 0.0, correlation5 = 0.0;
			for (a = 0; a < Ng; a++) {
				for (b = 0; b < Ng; b++) {
					correlation1 = correlation1 + ((a - meanx1) * (b - meany1) * glcm1[a][b] / (stdevx1 * stdevy1));
					correlation2 = correlation2 + ((a - meanx2) * (b - meany2) * glcm2[a][b] / (stdevx2 * stdevy2));
					correlation3 = correlation3 + ((a - meanx3) * (b - meany3) * glcm3[a][b] / (stdevx3 * stdevy3));
					correlation4 = correlation4 + ((a - meanx4) * (b - meany4) * glcm4[a][b] / (stdevx4 * stdevy4));
					correlation5 = correlation5 + ((a - meanx5) * (b - meany5) * glcm5[a][b] / (stdevx5 * stdevy5));
				}
			}
			rt.setValue("Correlation", row, correlation1);
			rt.setValue("Correlation", row + 1, correlation2);
			rt.setValue("Correlation", row + 2, correlation3);
			rt.setValue("Correlation", row + 3, correlation4);
			rt.setValue("Correlation", row + 4, correlation5);
		}

		// ===============================================================================================
		// This part calculates the inverse difference moment

		if (doIcalculateIDM == true) {
			double IDM1 = 0.0, IDM2 = 0.0, IDM3 = 0.0, IDM4 = 0.0, IDM5 = 0.0;
			for (a = 0; a < Ng; a++) {
				for (b = 0; b < Ng; b++) {
					IDM1 = IDM1 + (glcm1[a][b] / (1 + (a - b) * (a - b)));
					IDM2 = IDM2 + (glcm2[a][b] / (1 + (a - b) * (a - b)));
					IDM3 = IDM3 + (glcm3[a][b] / (1 + (a - b) * (a - b)));
					IDM4 = IDM4 + (glcm4[a][b] / (1 + (a - b) * (a - b)));
					IDM5 = IDM5 + (glcm5[a][b] / (1 + (a - b) * (a - b)));
				}
			}
			rt.setValue("IDM", row, IDM1);
			rt.setValue("IDM", row + 1, IDM2);
			rt.setValue("IDM", row + 2, IDM3);
			rt.setValue("IDM", row + 3, IDM4);
			rt.setValue("IDM", row + 4, IDM5);
		}

		// ===============================================================================================
		// This part calculates the entropy
		if (doIcalculateEntropy == true) {
			double entropy1 = 0.0, entropy2 = 0.0, entropy3 = 0.0, entropy4 = 0.0, entropy5 = 0.0;
			for (a = 0; a < Ng; a++) {
				for (b = 0; b < Ng; b++) {
					if (glcm1[a][b] != 0) {
						entropy1 = entropy1 - (glcm1[a][b] * (Math.log(glcm1[a][b])));
					}
					if (glcm2[a][b] != 0) {
						entropy2 = entropy2 - (glcm2[a][b] * (Math.log(glcm2[a][b])));
					}
					if (glcm3[a][b] != 0) {
						entropy3 = entropy3 - (glcm3[a][b] * (Math.log(glcm3[a][b])));
					}
					if (glcm4[a][b] != 0) {
						entropy4 = entropy4 - (glcm4[a][b] * (Math.log(glcm4[a][b])));
					}
					if (glcm5[a][b] != 0) {
						entropy5 = entropy5 - (glcm5[a][b] * (Math.log(glcm5[a][b])));
					}
				}
			}
			rt.setValue("Entropy", row, entropy1);
			rt.setValue("Entropy", row + 1, entropy2);
			rt.setValue("Entropy", row + 2, entropy3);
			rt.setValue("Entropy", row + 3, entropy4);
			rt.setValue("Entropy", row + 4, entropy5);
		}

		// ===============================================================================================
		// This part calculates the Dissimilarity
		if (doIcalculateDissimilarity == true) {
			double dissimilarity1 = 0.0, dissimilarity2 = 0.0, dissimilarity3 = 0.0, dissimilarity4 = 0.0,
					dissimilarity5 = 0.0;
			for (a = 0; a < Ng; a++) {
				for (b = 0; b < Ng; b++) {
					if (glcm1[a][b] != 0) {
						dissimilarity1 = dissimilarity1 + (Math.abs(a - b) * glcm1[a][b]);
					}
					if (glcm2[a][b] != 0) {
						dissimilarity2 = dissimilarity2 + (Math.abs(a - b) * glcm2[a][b]);
					}
					if (glcm3[a][b] != 0) {
						dissimilarity3 = dissimilarity3 + (Math.abs(a - b) * glcm3[a][b]);
					}
					if (glcm4[a][b] != 0) {
						dissimilarity4 = dissimilarity4 + (Math.abs(a - b) * glcm4[a][b]);
					}
					if (glcm5[a][b] != 0) {
						dissimilarity5 = dissimilarity5 + (Math.abs(a - b) * glcm5[a][b]);
					}
				}
			}
			rt.setValue("Dissimilarity", row, dissimilarity1);
			rt.setValue("Dissimilarity", row + 1, dissimilarity2);
			rt.setValue("Dissimilarity", row + 2, dissimilarity3);
			rt.setValue("Dissimilarity", row + 3, dissimilarity4);
			rt.setValue("Dissimilarity", row + 4, dissimilarity5);
		}

		// ===============================================================================================
		// This part calculates the inverse difference
		if (doIcalculateINV == true) {
			double INV1 = 0.0, INV2 = 0.0, INV3 = 0.0, INV4 = 0.0, INV5 = 0.0;
			for (a = 0; a < Ng; a++) {
				for (b = 0; b < Ng; b++) {
					INV1 = INV1 + (glcm1[a][b] / (1 + Math.abs(a - b)));
					INV2 = INV2 + (glcm2[a][b] / (1 + Math.abs(a - b)));
					INV3 = INV3 + (glcm3[a][b] / (1 + Math.abs(a - b)));
					INV4 = INV4 + (glcm4[a][b] / (1 + Math.abs(a - b)));
					INV5 = INV5 + (glcm5[a][b] / (1 + Math.abs(a - b)));
				}
			}
			rt.setValue("INV", row, INV1);
			rt.setValue("INV", row + 1, INV2);
			rt.setValue("INV", row + 2, INV3);
			rt.setValue("INV", row + 3, INV4);
			rt.setValue("INV", row + 4, INV5);
		}

		// ===============================================================================================
		// This part calculates the Variance
		if (doIcalculateVariance == true) {
			double variance1 = 0.0, variance2 = 0.0, variance3 = 0.0, variance4 = 0.0, variance5 = 0.0;
			for (a = 0; a < Ng; a++) {
				for (b = 0; b < Ng; b++) {
					if (glcm1[a][b] != 0) {
						variance1 = variance1 + ((a - meanx1) * (a - meanx1) * glcm1[a][b]);
					}
					if (glcm2[a][b] != 0) {
						variance2 = variance2 + ((a - meanx2) * (a - meanx2) * glcm2[a][b]);
					}
					if (glcm3[a][b] != 0) {
						variance3 = variance3 + ((a - meanx3) * (a - meanx3) * glcm3[a][b]);
					}
					if (glcm4[a][b] != 0) {
						variance4 = variance4 + ((a - meanx4) * (a - meanx4) * glcm4[a][b]);
					}
					if (glcm5[a][b] != 0) {
						variance5 = variance5 + ((a - meanx5) * (a - meanx5) * glcm5[a][b]);
					}
				}
			}
			for (a = 0; a < Ng; a++) {
				for (b = 0; b < Ng; b++) {
					if (glcm1[a][b] != 0) {
						variance1 = variance1 + ((b - meanx1) * (b - meanx1) * glcm1[a][b]);
					}
					if (glcm2[a][b] != 0) {
						variance2 = variance2 + ((b - meanx2) * (b - meanx2) * glcm2[a][b]);
					}
					if (glcm3[a][b] != 0) {
						variance3 = variance3 + ((b - meanx3) * (b - meanx3) * glcm3[a][b]);
					}
					if (glcm4[a][b] != 0) {
						variance4 = variance4 + ((b - meanx4) * (b - meanx4) * glcm4[a][b]);
					}
					if (glcm5[a][b] != 0) {
						variance5 = variance5 + ((b - meanx5) * (b - meanx5) * glcm5[a][b]);
					}
				}
			}
			rt.setValue("Variance", row, variance1);
			rt.setValue("Variance", row + 1, variance2);
			rt.setValue("Variance", row + 2, variance3);
			rt.setValue("Variance", row + 3, variance4);
			rt.setValue("Variance", row + 4, variance5);
		}

		// ===============================================================================================
		// This part calculates Cluster Shade
		if (doIcalculateClusterShade == true) {
			double clusterShade1 = 0.0, clusterShade2 = 0.0, clusterShade3 = 0.0, clusterShade4 = 0.0,
					clusterShade5 = 0.0;
			for (a = 0; a < Ng; a++) {
				for (b = 0; b < Ng; b++) {
					if (glcm1[a][b] != 0) {
						clusterShade1 = clusterShade1 + ((a + b - meanx1 - meany1) * (a + b - meanx1 - meany1)
								* (a + b - meanx1 - meany1) * glcm1[a][b]);
					}
					if (glcm2[a][b] != 0) {
						clusterShade2 = clusterShade2 + ((a + b - meanx2 - meany2) * (a + b - meanx2 - meany2)
								* (a + b - meanx2 - meany2) * glcm2[a][b]);
					}
					if (glcm3[a][b] != 0) {
						clusterShade3 = clusterShade3 + ((a + b - meanx3 - meany3) * (a + b - meanx3 - meany3)
								* (a + b - meanx3 - meany3) * glcm3[a][b]);
					}
					if (glcm4[a][b] != 0) {
						clusterShade4 = clusterShade4 + ((a + b - meanx4 - meany4) * (a + b - meanx4 - meany4)
								* (a + b - meanx4 - meany4) * glcm4[a][b]);
					}
					if (glcm5[a][b] != 0) {
						clusterShade5 = clusterShade5 + ((a + b - meanx5 - meany5) * (a + b - meanx5 - meany5)
								* (a + b - meanx5 - meany5) * glcm5[a][b]);
					}
				}
			}
			rt.setValue("CS", row, clusterShade1);
			rt.setValue("CS", row + 1, clusterShade2);
			rt.setValue("CS", row + 2, clusterShade3);
			rt.setValue("CS", row + 3, clusterShade4);
			rt.setValue("CS", row + 4, clusterShade5);
		}

		// ===============================================================================================
		// This part calculates Cluster Prominence
		if (doIcalculateClusterProminence == true) {
			double clusterProminence1 = 0.0, clusterProminence2 = 0.0, clusterProminence3 = 0.0,
					clusterProminence4 = 0.0, clusterProminence5 = 0.0;
			for (a = 0; a < Ng; a++) {
				for (b = 0; b < Ng; b++) {
					if (glcm1[a][b] != 0) {
						clusterProminence1 = clusterProminence1 + ((a + b - meanx1 - meany1) * (a + b - meanx1 - meany1)
								* (a + b - meanx1 - meany1) * (a + b - meanx1 - meany1) * glcm1[a][b]);
					}
					if (glcm2[a][b] != 0) {
						clusterProminence2 = clusterProminence2 + ((a + b - meanx2 - meany2) * (a + b - meanx2 - meany2)
								* (a + b - meanx2 - meany2) * (a + b - meanx2 - meany2) * glcm2[a][b]);
					}
					if (glcm3[a][b] != 0) {
						clusterProminence3 = clusterProminence3 + ((a + b - meanx3 - meany3) * (a + b - meanx3 - meany3)
								* (a + b - meanx3 - meany3) * (a + b - meanx3 - meany3) * glcm3[a][b]);
					}
					if (glcm4[a][b] != 0) {
						clusterProminence4 = clusterProminence4 + ((a + b - meanx4 - meany4) * (a + b - meanx4 - meany4)
								* (a + b - meanx4 - meany4) * (a + b - meanx4 - meany4) * glcm4[a][b]);
					}
					if (glcm5[a][b] != 0) {
						clusterProminence5 = clusterProminence5 + ((a + b - meanx5 - meany5) * (a + b - meanx5 - meany5)
								* (a + b - meanx5 - meany5) * (a + b - meanx5 - meany5) * glcm5[a][b]);
					}
				}
			}
			rt.setValue("CP", row, clusterProminence1);
			rt.setValue("CP", row + 1, clusterProminence2);
			rt.setValue("CP", row + 2, clusterProminence3);
			rt.setValue("CP", row + 3, clusterProminence4);
			rt.setValue("CP", row + 4, clusterProminence5);
		}

		// ===============================================================================================
		// This part calculates the Inverse Difference Normalized (INN)

		if (doIcalculateINN == true) {
			double INN1 = 0.0, INN2 = 0.0, INN3 = 0.0, INN4 = 0.0, INN5 = 0.0;
			for (a = 0; a < Ng; a++) {
				for (b = 0; b < Ng; b++) {
					INN1 = INN1 + (glcm1[a][b] / (1 + Math.abs(a - b) * Math.abs(a - b) / Ng / Ng));
					INN2 = INN2 + (glcm2[a][b] / (1 + Math.abs(a - b) * Math.abs(a - b) / Ng / Ng));
					INN3 = INN3 + (glcm3[a][b] / (1 + Math.abs(a - b) * Math.abs(a - b) / Ng / Ng));
					INN4 = INN4 + (glcm4[a][b] / (1 + Math.abs(a - b) * Math.abs(a - b) / Ng / Ng));
					INN5 = INN5 + (glcm5[a][b] / (1 + Math.abs(a - b) * Math.abs(a - b) / Ng / Ng));
				}
			}
			rt.setValue("INN", row, INN1);
			rt.setValue("INN", row + 1, INN2);
			rt.setValue("INN", row + 2, INN3);
			rt.setValue("INN", row + 3, INN4);
			rt.setValue("INN", row + 4, INN5);
		}

		// ===============================================================================================
		// This part calculates the Inverse Difference Moment Normalized (IDN)
		if (doIcalculateIDN == true) {
			double IDN1 = 0.0, IDN2 = 0.0, IDN3 = 0.0, IDN4 = 0.0, IDN5 = 0.0;
			for (a = 0; a < Ng; a++) {
				for (b = 0; b < Ng; b++) {
					IDN1 = IDN1 + (glcm1[a][b] / (1 + Math.abs(a - b) * Math.abs(a - b) / Ng / Ng));
					IDN2 = IDN2 + (glcm2[a][b] / (1 + Math.abs(a - b) * Math.abs(a - b) / Ng / Ng));
					IDN3 = IDN3 + (glcm3[a][b] / (1 + Math.abs(a - b) * Math.abs(a - b) / Ng / Ng));
					IDN4 = IDN4 + (glcm4[a][b] / (1 + Math.abs(a - b) * Math.abs(a - b) / Ng / Ng));
					IDN5 = IDN5 + (glcm5[a][b] / (1 + Math.abs(a - b) * Math.abs(a - b) / Ng / Ng));
				}
			}
			rt.setValue("IDN", row, IDN1);
			rt.setValue("IDN", row + 1, IDN2);
			rt.setValue("IDN", row + 2, IDN3);
			rt.setValue("IDN", row + 3, IDN4);
			rt.setValue("IDN", row + 4, IDN5);
		}

		// ===============================================================================================
		// This part calculates the Maximum Probability
		if (doIcalculateMaxProb == true) {
			double maxProb1 = 0.0, maxProb2 = 0.0, maxProb3 = 0.0, maxProb4 = 0.0, maxProb5 = 0.0;
			for (a = 0; a < Ng; a++) {
				for (b = 0; b < Ng; b++) {
					if (maxProb1 < glcm1[a][b]) {
						maxProb1 = glcm1[a][b];
					}
					if (maxProb2 < glcm2[a][b]) {
						maxProb2 = glcm2[a][b];
					}
					if (maxProb3 < glcm3[a][b]) {
						maxProb3 = glcm3[a][b];
					}
					if (maxProb4 < glcm4[a][b]) {
						maxProb4 = glcm4[a][b];
					}
					if (maxProb5 < glcm5[a][b]) {
						maxProb5 = glcm5[a][b];
					}
				}
			}
			rt.setValue("MaxProb", row, maxProb1);
			rt.setValue("MaxProb", row + 1, maxProb2);
			rt.setValue("MaxProb", row + 2, maxProb3);
			rt.setValue("MaxProb", row + 3, maxProb4);
			rt.setValue("MaxProb", row + 4, maxProb5);
		}

		// ===============================================================================================
		// This part calculates the Sum Average
		if (doIcalculateSumAvg == true) {
			double sumAvg1 = 0.0, sumAvg2 = 0.0, sumAvg3 = 0.0, sumAvg4 = 0.0, sumAvg5 = 0.0;
			for (k = 2; k <= 2 * Ng; k++) {
				sumAvg1 = sumAvg1 + k * p_x_plus_y1[k - 2];
				sumAvg2 = sumAvg2 + k * p_x_plus_y2[k - 2];
				sumAvg3 = sumAvg3 + k * p_x_plus_y3[k - 2];
				sumAvg4 = sumAvg4 + k * p_x_plus_y4[k - 2];
				sumAvg5 = sumAvg5 + k * p_x_plus_y5[k - 2];
			}
			rt.setValue("SumAvg", row, sumAvg1);
			rt.setValue("SumAvg", row + 1, sumAvg2);
			rt.setValue("SumAvg", row + 2, sumAvg3);
			rt.setValue("SumAvg", row + 3, sumAvg4);
			rt.setValue("SumAvg", row + 4, sumAvg5);
		}

		// ===============================================================================================
		// This part calculates the Sum Entropy
		double sumEnth1 = 0.0, sumEnth2 = 0.0, sumEnth3 = 0.0, sumEnth4 = 0.0, sumEnth5 = 0.0;
		for (k = 2; k <= 2 * Ng; k++) {
			if (p_x_plus_y1[k - 2] != 0) {
				sumEnth1 = sumEnth1 - (p_x_plus_y1[k - 2] * (Math.log(p_x_plus_y1[k - 2])));
			}
			if (p_x_plus_y2[k - 2] != 0) {
				sumEnth2 = sumEnth2 - (p_x_plus_y2[k - 2] * (Math.log(p_x_plus_y2[k - 2])));
			}
			if (p_x_plus_y3[k - 2] != 0) {
				sumEnth3 = sumEnth3 - (p_x_plus_y3[k - 2] * (Math.log(p_x_plus_y3[k - 2])));
			}
			if (p_x_plus_y4[k - 2] != 0) {
				sumEnth4 = sumEnth4 - (p_x_plus_y4[k - 2] * (Math.log(p_x_plus_y4[k - 2])));
			}
			if (p_x_plus_y5[k - 2] != 0) {
				sumEnth5 = sumEnth5 - (p_x_plus_y5[k - 2] * (Math.log(p_x_plus_y5[k - 2])));
			}
		}
		if (doIcalculateSumEnth == true) {
			rt.setValue("SumEnth", row, sumEnth1);
			rt.setValue("SumEnth", row + 1, sumEnth2);
			rt.setValue("SumEnth", row + 2, sumEnth3);
			rt.setValue("SumEnth", row + 3, sumEnth4);
			rt.setValue("SumEnth", row + 4, sumEnth5);
		}

		// ===============================================================================================
		// This part calculates the Sum Variance
		if (doIcalculateSumVar == true) {
			double sumVar1 = 0.0, sumVar2 = 0.0, sumVar3 = 0.0, sumVar4 = 0.0, sumVar5 = 0.0;
			for (k = 2; k <= 2 * Ng; k++) {
				sumVar1 = sumVar1 + (k - sumEnth1) * (k - sumEnth1) * p_x_plus_y1[k - 2];
				sumVar2 = sumVar2 + (k - sumEnth2) * (k - sumEnth2) * p_x_plus_y2[k - 2];
				sumVar3 = sumVar3 + (k - sumEnth3) * (k - sumEnth3) * p_x_plus_y3[k - 2];
				sumVar4 = sumVar4 + (k - sumEnth4) * (k - sumEnth4) * p_x_plus_y4[k - 2];
				sumVar5 = sumVar5 + (k - sumEnth5) * (k - sumEnth5) * p_x_plus_y5[k - 2];
			}
			rt.setValue("SumVar", row, sumVar1);
			rt.setValue("SumVar", row + 1, sumVar2);
			rt.setValue("SumVar", row + 2, sumVar3);
			rt.setValue("SumVar", row + 3, sumVar4);
			rt.setValue("SumVar", row + 4, sumVar5);
		}

		// ===============================================================================================
		// This part calculates the Difference Variance
		if (doIcalculateDiffVar == true) {
			double diffVar1 = 0.0, diffVar2 = 0.0, diffVar3 = 0.0, diffVar4 = 0.0, diffVar5 = 0.0;
			for (k = 0; k <= Ng - 1; k++) {
				diffVar1 = diffVar1 + (k - mean_p_x_minus_y1) * (k - mean_p_x_minus_y1) * p_x_minus_y1[k];
				diffVar2 = diffVar2 + (k - mean_p_x_minus_y2) * (k - mean_p_x_minus_y2) * p_x_minus_y2[k];
				diffVar3 = diffVar3 + (k - mean_p_x_minus_y3) * (k - mean_p_x_minus_y3) * p_x_minus_y3[k];
				diffVar4 = diffVar4 + (k - mean_p_x_minus_y4) * (k - mean_p_x_minus_y4) * p_x_minus_y4[k];
				diffVar5 = diffVar5 + (k - mean_p_x_minus_y5) * (k - mean_p_x_minus_y5) * p_x_minus_y5[k];
			}
			rt.setValue("DiffVar", row, diffVar1);
			rt.setValue("DiffVar", row + 1, diffVar2);
			rt.setValue("DiffVar", row + 2, diffVar3);
			rt.setValue("DiffVar", row + 3, diffVar4);
			rt.setValue("DiffVar", row + 4, diffVar5);
		}

		// ===============================================================================================
		// This part calculates the Difference Entropy
		if (doIcalculateDiffEnth == true) {
			double diffEnth1 = 0.0, diffEnth2 = 0.0, diffEnth3 = 0.0, diffEnth4 = 0.0, diffEnth5 = 0.0;
			for (k = 0; k <= Ng - 1; k++) {
				if (p_x_minus_y1[k] != 0) {
					diffEnth1 = diffEnth1 - p_x_minus_y1[k] * Math.log(p_x_minus_y1[k]);
				}
				if (p_x_minus_y2[k] != 0) {
					diffEnth2 = diffEnth2 - p_x_minus_y2[k] * Math.log(p_x_minus_y2[k]);
				}
				if (p_x_minus_y3[k] != 0) {
					diffEnth3 = diffEnth3 - p_x_minus_y3[k] * Math.log(p_x_minus_y3[k]);
				}
				if (p_x_minus_y4[k] != 0) {
					diffEnth4 = diffEnth4 - p_x_minus_y4[k] * Math.log(p_x_minus_y4[k]);
				}
				if (p_x_minus_y5[k] != 0) {
					diffEnth5 = diffEnth5 - p_x_minus_y5[k] * Math.log(p_x_minus_y5[k]);
				}
			}
			rt.setValue("DiffEnth", row, diffEnth1);
			rt.setValue("DiffEnth", row + 1, diffEnth2);
			rt.setValue("DiffEnth", row + 2, diffEnth3);
			rt.setValue("DiffEnth", row + 3, diffEnth4);
			rt.setValue("DiffEnth", row + 4, diffEnth5);
		}

		// ===============================================================================================
		// This part calculates the Difference Entropy
		if (doCheckCounts == true) {
			rt.setValue("Counts", row, pixelCounter1);
			rt.setValue("Counts", row + 1, pixelCounter2);
			rt.setValue("Counts", row + 2, pixelCounter3);
			rt.setValue("Counts", row + 3, pixelCounter4);
			rt.setValue("Counts", row + 4, pixelCounter5);
		}

		// ===============================================================================================
		// Show the title world
		rt.show("Results");
	}

	// This part is the implementation of the Dialog box (called gd here)
	boolean showDialog() {
		GenericDialog gd = new GenericDialog("Textural features based on GLCM Texture 0.4 (developed by FEMH)");

		Font dialog = new Font("Helvetica", Font.BOLD, 14);

		gd.addMessage("I. This plug-in calculates textural features is based on Gray Level Correlation Matrices.",
				dialog);

		gd.addMessage("II. Basic settings:", dialog);

		gd.addNumericField("Enter the size of the step in pixels", step, 0);

		// String[] stepOptions = { "0 degrees", "45 degrees", "90 degrees",
		// "135 degrees", "Average of all angles" };
		// gd.addChoice("Select the direction of the step", stepOptions,
		// selectedStep);

		gd.addCheckbox("Show GLCM Matrix Means, STDs and Sum of All Elements", doIshowBasic);

		gd.addMessage("III. Check in the following boxes for the parameters you want to compute and click OK:", dialog);

		gd.addCheckbox(" 1.  Angular Second Moment (ASM)", doIcalculateASM);
		gd.addCheckbox(" 2.  Contrast", doIcalculateContrast);
		gd.addCheckbox(" 3.  Correlation", doIcalculateCorrelation);
		gd.addCheckbox(" 4.  Inverse Difference Moment (IDM)", doIcalculateIDM);
		gd.addCheckbox(" 5.  Entropy", doIcalculateEntropy);
		gd.addCheckbox(" 6.  Dissimilarity", doIcalculateDissimilarity);
		gd.addCheckbox(" 7.  Inverse Difference (INV)", doIcalculateINV);
		gd.addCheckbox(" 8.  Variance", doIcalculateVariance);
		gd.addCheckbox(" 9.  Cluster Shade (CS)", doIcalculateClusterShade);
		gd.addCheckbox("10. Cluster Prominence (CP)", doIcalculateClusterProminence);
		gd.addCheckbox("11. Inverse Difference Normalized (INN)", doIcalculateINN);
		gd.addCheckbox("12. Inverse Difference Moment Normalized (IDN)", doIcalculateIDN);
		gd.addCheckbox("13. Maximum Probability (MaxProb)", doIcalculateMaxProb);
		gd.addCheckbox("14. Sum Average (SumAvg)", doIcalculateSumAvg);
		gd.addCheckbox("15. Sum Entropy (SumEnth)", doIcalculateSumEnth);
		gd.addCheckbox("16. Sum Variance (SumVar)", doIcalculateSumVar);
		gd.addCheckbox("17. Difference Variance (DiffVar)", doIcalculateDiffVar);
		gd.addCheckbox("18. Difference Entropy (DiffEnth)", doIcalculateDiffEnth);
		gd.addCheckbox("19. Check pixels data counts", doCheckCounts);

		gd.showDialog();
		if (gd.wasCanceled())
			return false;

		step = (int) gd.getNextNumber();
		selectedStep = gd.getNextChoice();
		doIshowBasic = gd.getNextBoolean();
		doIcalculateASM = gd.getNextBoolean();
		doIcalculateContrast = gd.getNextBoolean();
		doIcalculateCorrelation = gd.getNextBoolean();
		doIcalculateIDM = gd.getNextBoolean();
		doIcalculateEntropy = gd.getNextBoolean();
		doIcalculateDissimilarity = gd.getNextBoolean();
		doIcalculateINV = gd.getNextBoolean();
		doIcalculateVariance = gd.getNextBoolean();
		doIcalculateClusterShade = gd.getNextBoolean();
		doIcalculateClusterProminence = gd.getNextBoolean();
		doIcalculateINN = gd.getNextBoolean();
		doIcalculateIDN = gd.getNextBoolean();
		doIcalculateMaxProb = gd.getNextBoolean();
		doIcalculateSumAvg = gd.getNextBoolean();
		doIcalculateSumEnth = gd.getNextBoolean();
		doIcalculateSumVar = gd.getNextBoolean();
		doIcalculateDiffVar = gd.getNextBoolean();
		doIcalculateDiffEnth = gd.getNextBoolean();
		doCheckCounts = gd.getNextBoolean();

		return true;
	}

}
