// The edge map shown over the image (View ▸ Show Edge Map) and the smoothing shared with the livewire tool.

import type { EdgeMethod, GradientStatsResponse } from '@glcm/api';
import { create } from 'zustand';
import { useViewer } from '../stores/viewerStore';

export interface EdgeLimits {
  low: number;
  high: number;
}

export const DEFAULT_EDGE_SIGMA = 1;
/** Canny's low threshold as a share of the high one when chosen automatically */
const CANNY_LOW_SHARE = 0.4;

/**
 * Limits chosen from the gradient statistics: for Sobel, 0 to the 99th percentile; for Canny, the 95th percentile as the
 * high threshold and 40 % of it as the low one. Always low < high for Sobel, so a flat image still gets a valid window.
 */
export function autoEdgeLimits(method: EdgeMethod, statistics: GradientStatsResponse): EdgeLimits {
  if (method === 'sobel') {
    return { low: 0, high: Math.max(statistics.percentiles['99'], Number.EPSILON) };
  }
  const high = statistics.percentiles['95'];
  return { low: high * CANNY_LOW_SHARE, high };
}

/** Rounds a limit to four significant digits for display and requests */
export function roundLimit(value: number): number {
  return value === 0 ? 0 : Number(value.toPrecision(4));
}

export interface EdgeMapState {
  shown: boolean;
  method: EdgeMethod;
  /** Gaussian smoothing in pixels, also used by the livewire tool */
  sigma: number;
  /** Chosen by the user; null uses autoEdgeLimits */
  limits: EdgeLimits | null;
  opacity: number;
  setShown(shown: boolean): void;
  setMethod(method: EdgeMethod): void;
  setSigma(sigma: number): void;
  setLimits(limits: EdgeLimits | null): void;
  setOpacity(opacity: number): void;
}

export const useEdgeMap = create<EdgeMapState>()((set) => ({
  shown: false,
  method: 'canny',
  sigma: DEFAULT_EDGE_SIGMA,
  limits: null,
  opacity: 0.85,
  setShown: (shown) => set({ shown }),
  // Limits mean different things for the two methods, and depend on the smoothing
  setMethod: (method) => set({ method, limits: null }),
  setSigma: (sigma) => set({ sigma: Math.min(10, Math.max(0, sigma)), limits: null }),
  setLimits: (limits) => set({ limits }),
  setOpacity: (opacity) => set({ opacity: Math.min(1, Math.max(0, opacity)) }),
}));

// Limits belong to one image
useViewer.subscribe((state, previous) => {
  if (state.image?.info.imageId !== previous.image?.info.imageId) {
    useEdgeMap.getState().setLimits(null);
  }
});
