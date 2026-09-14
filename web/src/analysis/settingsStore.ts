// Last-used analysis settings, kept in localStorage (doc/ui-design-plan.md, section 5.1).

import type { AnalysisSettings } from '@glcm/api';
import { create } from 'zustand';
import { persist } from 'zustand/middleware';
import { safeStorage } from '../stores/preferences';

export interface SettingsState {
  /** null until initialized from the catalog */
  settings: AnalysisSettings | null;
  setSettings(settings: AnalysisSettings): void;
  update(change: (settings: AnalysisSettings) => AnalysisSettings): void;
}

export const useAnalysisSettings = create<SettingsState>()(
  persist(
    (set, get) => ({
      settings: null,
      setSettings: (settings) => set({ settings }),
      update: (change) => {
        const { settings } = get();
        if (settings) {
          set({ settings: change(settings) });
        }
      },
    }),
    { name: 'glcm.analysisSettings', version: 1, storage: safeStorage },
  ),
);
