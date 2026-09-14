import { create } from 'zustand';

export type ModalName = 'imageInfo' | 'preferences' | 'shortcuts' | 'about' | 'samples';

export interface UiState {
  modal: ModalName | null;
  windowPanelOpen: boolean;
  /** Incremented by View ▸ Reset Layout to remount the panel groups */
  layoutVersion: number;
  /** Incremented to ask the app to show the file dialog */
  openFileRequest: number;
  setModal(modal: ModalName | null): void;
  setWindowPanelOpen(open: boolean): void;
  resetLayout(): void;
  requestOpenFile(): void;
}

export const useUi = create<UiState>()((set, get) => ({
  modal: null,
  windowPanelOpen: false,
  layoutVersion: 0,
  openFileRequest: 0,
  setModal: (modal) => set({ modal }),
  setWindowPanelOpen: (windowPanelOpen) => set({ windowPanelOpen }),
  resetLayout: () => set({ layoutVersion: get().layoutVersion + 1 }),
  requestOpenFile: () => set({ openFileRequest: get().openFileRequest + 1 }),
}));

export const MOD_KEY = typeof navigator !== 'undefined' && /Mac|iPhone|iPad/.test(navigator.platform) ? '⌘' : 'Ctrl+';
