// Last-used analysis settings, kept in localStorage (doc/ui-design-plan.md, section 5.1). Changes made by the user can be
// undone; the history stays in memory only.

import type { AnalysisSettings } from '@glcm/api';
import { create } from 'zustand';
import { persist } from 'zustand/middleware';
import { safeStorage } from '../stores/preferences';

/** Settings changes that can be undone */
export const SETTINGS_HISTORY_LIMIT = 100;

/**
 * How setSettings treats the undo history: "keep" it (defaults, fitting the settings to an image), "record" the change
 * as a step (Reset to defaults), or "clear" it (opening a project)
 */
export type SettingsHistory = 'keep' | 'record' | 'clear';

export interface SettingsState {
  /** null until initialized from the catalog */
  settings: AnalysisSettings | null;
  past: AnalysisSettings[];
  future: AnalysisSettings[];
  setSettings(settings: AnalysisSettings, options?: { history?: SettingsHistory }): void;
  /** A change by the user; recorded for undo when it changes the settings */
  update(change: (settings: AnalysisSettings) => AnalysisSettings): void;
  undo(): void;
  redo(): void;
}

const same = (a: AnalysisSettings, b: AnalysisSettings) => JSON.stringify(a) === JSON.stringify(b);

export const useAnalysisSettings = create<SettingsState>()(
  persist(
    (set, get) => {
      const record = (next: AnalysisSettings) => {
        const { settings, past } = get();
        if (settings && !same(settings, next)) {
          set({ settings: next, past: [...past, settings].slice(-SETTINGS_HISTORY_LIMIT), future: [] });
        }
      };

      return {
        settings: null,
        past: [],
        future: [],
        setSettings: (settings, { history = 'keep' } = {}) => {
          if (history === 'record') {
            record(settings);
          } else if (history === 'clear') {
            set({ settings, past: [], future: [] });
          } else {
            set({ settings });
          }
        },
        update: (change) => {
          const { settings } = get();
          if (settings) {
            record(change(settings));
          }
        },
        undo: () => {
          const { settings, past, future } = get();
          const previous = past[past.length - 1];
          if (settings && previous) {
            set({ settings: previous, past: past.slice(0, -1), future: [settings, ...future] });
          }
        },
        redo: () => {
          const { settings, past, future } = get();
          const [next, ...rest] = future;
          if (settings && next) {
            set({ settings: next, past: [...past, settings].slice(-SETTINGS_HISTORY_LIMIT), future: rest });
          }
        },
      };
    },
    // Only the settings are kept across sessions, not the undo history
    { name: 'glcm.analysisSettings', version: 1, storage: safeStorage, partialize: (state) => ({ settings: state.settings }) },
  ),
);
