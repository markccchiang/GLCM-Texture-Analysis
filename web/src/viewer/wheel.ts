// Mouse wheel and trackpad classification (doc/ui-design-plan.md, section 6.1).
//
// Browsers deliver mouse wheels, trackpad scrolling and trackpad pinches all as `wheel` events. Pinch gestures set
// ctrlKey. The "auto" behaviour treats events that look like a mouse wheel as zoom and everything else as pan; the
// preference can force either.

export type ScrollBehaviour = 'auto' | 'zoom' | 'pan';

export interface WheelInput {
  deltaX: number;
  deltaY: number;
  /** 0 = pixels, 1 = lines, 2 = pages */
  deltaMode: number;
  ctrlKey: boolean;
  metaKey: boolean;
}

export type WheelAction = { kind: 'zoom'; factor: number } | { kind: 'pan'; dx: number; dy: number };

const LINE_PIXELS = 16;
const PAGE_PIXELS = 800;
const MOUSE_WHEEL_MIN_STEP = 40;

function toPixels(delta: number, deltaMode: number): number {
  if (deltaMode === 1) {
    return delta * LINE_PIXELS;
  }
  if (deltaMode === 2) {
    return delta * PAGE_PIXELS;
  }
  return delta;
}

/** Line or page units, or large whole-number vertical steps: typical of mouse wheels rather than trackpads */
export function looksLikeMouseWheel(event: WheelInput): boolean {
  if (event.deltaMode !== 0) {
    return true;
  }
  if (event.deltaX !== 0) {
    return false;
  }
  return Number.isInteger(event.deltaY) && Math.abs(event.deltaY) >= MOUSE_WHEEL_MIN_STEP;
}

/**
 * Zoom factor (> 1 zooms in) or pan offset (screen pixels to move the image by) for a wheel event.
 * - ctrlKey (trackpad pinch) or metaKey (⌘ + scroll): always zoom
 * - "zoom": zoom; "pan": pan; "auto": zoom for mouse wheels, pan for trackpad scrolling
 */
export function classifyWheel(event: WheelInput, behaviour: ScrollBehaviour): WheelAction {
  const dx = toPixels(event.deltaX, event.deltaMode);
  const dy = toPixels(event.deltaY, event.deltaMode);

  const forcedZoom = event.ctrlKey || event.metaKey;
  const zoom = forcedZoom || behaviour === 'zoom' || (behaviour === 'auto' && looksLikeMouseWheel(event));
  if (!zoom) {
    return { kind: 'pan', dx: -dx, dy: -dy };
  }

  // Pinch deltas are small and frequent, mouse wheel steps large
  const sensitivity = event.ctrlKey ? 0.01 : 0.002;
  const factor = Math.min(2, Math.max(0.5, Math.exp(-dy * sensitivity)));
  return { kind: 'zoom', factor };
}
