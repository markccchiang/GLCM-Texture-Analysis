// User preferences, kept in localStorage (doc/ui-design-plan.md, section 5.1).

import { create } from 'zustand';
import { createJSONStorage, persist } from 'zustand/middleware';
import type { ScrollBehaviour } from '../viewer/wheel';

export interface PreferencesState {
  scrollBehaviour: ScrollBehaviour;
  /** Use the WebGL2 renderer when available (the lookup-table renderer otherwise) */
  useWebGl: boolean;
  setScrollBehaviour(scrollBehaviour: ScrollBehaviour): void;
  setUseWebGl(useWebGl: boolean): void;
}

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
      setScrollBehaviour: (scrollBehaviour) => set({ scrollBehaviour }),
      setUseWebGl: (useWebGl) => set({ useWebGl }),
    }),
    { name: 'glcm.preferences', version: 1, storage: safeStorage },
  ),
);
