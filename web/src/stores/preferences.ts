// User preferences, kept in localStorage (doc/ui-design-plan.md, section 5.1).

import { create } from 'zustand';
import { createJSONStorage, persist } from 'zustand/middleware';
import type { ScrollBehaviour } from '../viewer/wheel';

/** A display window saved by the user; offered for images of the same bit depth */
export interface WindowPreset {
  name: string;
  bitDepth: number;
  min: number;
  max: number;
}

export interface PreferencesState {
  scrollBehaviour: ScrollBehaviour;
  /** Use the WebGL2 renderer when available (the lookup-table renderer otherwise) */
  useWebGl: boolean;
  windowPresets: WindowPreset[];
  setScrollBehaviour(scrollBehaviour: ScrollBehaviour): void;
  setUseWebGl(useWebGl: boolean): void;
  /** Adds a preset, replacing one with the same name and bit depth */
  saveWindowPreset(preset: WindowPreset): void;
  removeWindowPreset(name: string, bitDepth: number): void;
}

const samePreset = (preset: WindowPreset, name: string, bitDepth: number) => preset.name === name && preset.bitDepth === bitDepth;

/** localStorage that silently does nothing when storage is blocked */
export const safeStorage = createJSONStorage(() => {
  try {
    const probe = '__glcm_probe__';
    window.localStorage.setItem(probe, probe);
    window.localStorage.removeItem(probe);
    return window.localStorage;
  } catch {
    return { getItem: () => null, setItem: () => undefined, removeItem: () => undefined };
  }
});

export const usePreferences = create<PreferencesState>()(
  persist(
    (set) => ({
      scrollBehaviour: 'auto',
      useWebGl: true,
      windowPresets: [],
      setScrollBehaviour: (scrollBehaviour) => set({ scrollBehaviour }),
      setUseWebGl: (useWebGl) => set({ useWebGl }),
      saveWindowPreset: (preset) =>
        set((state) => ({ windowPresets: [...state.windowPresets.filter((existing) => !samePreset(existing, preset.name, preset.bitDepth)), preset] })),
      removeWindowPreset: (name, bitDepth) =>
        set((state) => ({ windowPresets: state.windowPresets.filter((existing) => !samePreset(existing, name, bitDepth)) })),
    }),
    { name: 'glcm.preferences', version: 1, storage: safeStorage },
  ),
);
