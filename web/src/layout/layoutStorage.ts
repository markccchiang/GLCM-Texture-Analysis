// Panel layouts in localStorage under one prefix, so View ▸ Reset Layout can remove them all.

import type { LayoutStorage } from 'react-resizable-panels';

const PREFIX = 'glcm.layout:';

export const layoutStorage: LayoutStorage = {
  getItem(key) {
    try {
      return window.localStorage.getItem(PREFIX + key);
    } catch {
      return null;
    }
  },
  setItem(key, value) {
    try {
      window.localStorage.setItem(PREFIX + key, value);
    } catch {
      // Storage blocked or full: layouts are simply not remembered
    }
  },
};

export function clearStoredLayouts(): void {
  try {
    const keys = Array.from({ length: window.localStorage.length }, (_, i) => window.localStorage.key(i));
    for (const key of keys) {
      if (key?.startsWith(PREFIX)) {
        window.localStorage.removeItem(key);
      }
    }
  } catch {
    // Nothing stored
  }
}
