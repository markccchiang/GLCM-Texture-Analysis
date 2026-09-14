import { describe, expect, it } from 'vitest';
import { classifyWheel, looksLikeMouseWheel, type WheelInput } from './wheel';

// Recorded-style samples of typical devices
const MOUSE_WHEEL_LINES: WheelInput = { deltaX: 0, deltaY: 3, deltaMode: 1, ctrlKey: false, metaKey: false }; // Firefox
const MOUSE_WHEEL_PIXELS: WheelInput = { deltaX: 0, deltaY: -100, deltaMode: 0, ctrlKey: false, metaKey: false }; // Chrome
const TRACKPAD_SCROLL: WheelInput = { deltaX: 2.5, deltaY: 7.75, deltaMode: 0, ctrlKey: false, metaKey: false };
const TRACKPAD_VERTICAL: WheelInput = { deltaX: 0, deltaY: 4, deltaMode: 0, ctrlKey: false, metaKey: false };
const TRACKPAD_PINCH: WheelInput = { deltaX: 0, deltaY: -3.2, deltaMode: 0, ctrlKey: true, metaKey: false };

describe('looksLikeMouseWheel', () => {
  it('recognizes mouse wheels by units or large whole-number steps', () => {
    expect(looksLikeMouseWheel(MOUSE_WHEEL_LINES)).toBe(true);
    expect(looksLikeMouseWheel(MOUSE_WHEEL_PIXELS)).toBe(true);
    expect(looksLikeMouseWheel(TRACKPAD_SCROLL)).toBe(false);
    expect(looksLikeMouseWheel(TRACKPAD_VERTICAL)).toBe(false);
  });
});

describe('classifyWheel', () => {
  it('auto: zooms with mouse wheels and pans with trackpad scrolling', () => {
    const zoomIn = classifyWheel(MOUSE_WHEEL_PIXELS, 'auto');
    expect(zoomIn.kind).toBe('zoom');
    expect(zoomIn.kind === 'zoom' && zoomIn.factor).toBeGreaterThan(1);

    const zoomOut = classifyWheel(MOUSE_WHEEL_LINES, 'auto');
    expect(zoomOut.kind === 'zoom' && zoomOut.factor).toBeLessThan(1);

    expect(classifyWheel(TRACKPAD_SCROLL, 'auto')).toEqual({ kind: 'pan', dx: -2.5, dy: -7.75 });
  });

  it('pinch and ⌘ + scroll always zoom', () => {
    const pinch = classifyWheel(TRACKPAD_PINCH, 'pan');
    expect(pinch.kind).toBe('zoom');
    expect(pinch.kind === 'zoom' && pinch.factor).toBeCloseTo(Math.exp(0.032), 10);
    expect(classifyWheel({ ...TRACKPAD_SCROLL, metaKey: true }, 'pan').kind).toBe('zoom');
  });

  it('preferences force zoom or pan', () => {
    expect(classifyWheel(TRACKPAD_SCROLL, 'zoom').kind).toBe('zoom');
    expect(classifyWheel(MOUSE_WHEEL_LINES, 'pan')).toEqual({ kind: 'pan', dx: -0, dy: -48 });
  });

  it('limits the zoom factor of a single event', () => {
    const huge: WheelInput = { deltaX: 0, deltaY: -5000, deltaMode: 0, ctrlKey: false, metaKey: false };
    expect(classifyWheel(huge, 'zoom')).toEqual({ kind: 'zoom', factor: 2 });
    expect(classifyWheel({ ...huge, deltaY: 5000 }, 'zoom')).toEqual({ kind: 'zoom', factor: 0.5 });
  });
});
