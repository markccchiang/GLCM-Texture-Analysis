// Keyboard shortcuts of the image canvas (doc/ui-design-plan.md, section 6.1).

import type { Size } from './viewport';

export type ViewerAction =
  | { kind: 'zoomIn' }
  | { kind: 'zoomOut' }
  | { kind: 'zoom100' }
  | { kind: 'fit' }
  | { kind: 'toggleNavigator' }
  | { kind: 'zoomToSelection' }
  /** Move the view (the image moves on screen by dx, dy screen pixels) */
  | { kind: 'pan'; dx: number; dy: number }
  /** Move the selected ROIs by dx, dy image pixels */
  | { kind: 'nudge'; dx: number; dy: number };

export interface KeyInput {
  key: string;
  shiftKey: boolean;
  ctrlKey: boolean;
  metaKey: boolean;
  altKey: boolean;
}

export interface KeyContext {
  /** Whether ROIs are selected (arrow keys then move them instead of the view) */
  hasSelection: boolean;
  view: Size;
}

export const PAN_STEP = 50;
export const NUDGE_STEP = 1;
export const NUDGE_STEP_LARGE = 10;

const ARROWS: Record<string, [number, number]> = {
  ArrowLeft: [-1, 0],
  ArrowRight: [1, 0],
  ArrowUp: [0, -1],
  ArrowDown: [0, 1],
};

/**
 * The action for a key press, or null if the canvas does not handle it. Keys with ⌘, Ctrl or Alt are left to the
 * menus and the browser.
 */
export function keyToAction(input: KeyInput, context: KeyContext): ViewerAction | null {
  if (input.ctrlKey || input.metaKey || input.altKey) {
    return null;
  }

  const arrow = ARROWS[input.key];
  if (arrow) {
    const [horizontal, vertical] = arrow;
    if (context.hasSelection) {
      const step = input.shiftKey ? NUDGE_STEP_LARGE : NUDGE_STEP;
      return { kind: 'nudge', dx: horizontal * step, dy: vertical * step };
    }
    // Looking further left moves the image to the right on screen; Shift pans by one viewport
    const stepX = input.shiftKey ? context.view.width : PAN_STEP;
    const stepY = input.shiftKey ? context.view.height : PAN_STEP;
    return { kind: 'pan', dx: -horizontal * stepX, dy: -vertical * stepY };
  }

  switch (input.key) {
    case '+':
    case '=':
      return { kind: 'zoomIn' };
    case '-':
    case '_':
      return { kind: 'zoomOut' };
    case '1':
      return { kind: 'zoom100' };
    case '0':
      return { kind: 'fit' };
    case 'n':
    case 'N':
      return { kind: 'toggleNavigator' };
    case 'z':
    case 'Z':
      return context.hasSelection ? { kind: 'zoomToSelection' } : null;
    default:
      return null;
  }
}
