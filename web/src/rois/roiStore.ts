// ROIs of the open image, their selection and undo/redo (doc/ui-design-plan.md, section 6.2).
//
// History stores snapshots of the ROI list. Continuous edits (dragging, resizing) update the list live between
// beginEdit() and endEdit(), which records one step.

import type { RoiShape } from '@glcm/api';
import { create } from 'zustand';
import { roiColor, translateShape } from './geometry';

export interface ManagedRoi {
  id: string;
  name: string;
  color: string;
  visible: boolean;
  shape: RoiShape;
}

export const HISTORY_LIMIT = 200;

export interface RoiState {
  rois: ManagedRoi[];
  selectedIds: string[];
  hoveredId: string | null;
  /** Number used for the next default name "ROI n" */
  nextNumber: number;
  past: ManagedRoi[][];
  future: ManagedRoi[][];
  /** Snapshot taken by beginEdit() */
  editSnapshot: ManagedRoi[] | null;
  /** The shape just drawn, not yet in the ROI Manager (doc/ui-design-plan.md, section 6.2: active vs managed) */
  activeShape: RoiShape | null;

  setActiveShape(shape: RoiShape | null): void;
  /** Moves the active shape into the manager; returns its id, or null without an active shape */
  addActiveRoi(): string | null;
  addRoi(shape: RoiShape, name?: string): string;
  importRois(rois: Array<Omit<ManagedRoi, 'visible' | 'id'> & { id?: string; visible?: boolean }>): string[];
  deleteRois(ids: readonly string[]): void;
  duplicateRois(ids: readonly string[]): string[];
  renameRoi(id: string, name: string): void;
  /** Sets a colour "#RRGGBB"; undoable. Other values are ignored. */
  recolorRoi(id: string, color: string): void;
  replaceShape(id: string, shape: RoiShape): void;
  nudgeRois(ids: readonly string[], dx: number, dy: number): void;
  setVisible(id: string, visible: boolean): void;
  setAllVisible(visible: boolean): void;

  beginEdit(): void;
  updateShapeLive(id: string, shape: RoiShape): void;
  endEdit(): void;

  select(ids: readonly string[]): void;
  toggleSelected(id: string): void;
  selectRange(id: string): void;
  selectAll(): void;
  setHovered(id: string | null): void;

  undo(): void;
  redo(): void;
  /** Removes every ROI and the history (a new image was opened) */
  reset(): void;
}

function newRoiId(): string {
  return crypto.randomUUID();
}

const DUPLICATE_OFFSET = 10;

export const useRois = create<RoiState>()((set, get) => {
  /** Replaces the ROI list, recording the previous one as an undo step */
  const commit = (rois: ManagedRoi[], extra: Partial<RoiState> = {}) => {
    const { rois: previous, past } = get();
    set({ rois, past: [...past, previous].slice(-HISTORY_LIMIT), future: [], ...extra });
  };

  const validSelection = (rois: ManagedRoi[], selectedIds: string[]) => {
    const ids = new Set(rois.map((roi) => roi.id));
    return selectedIds.filter((id) => ids.has(id));
  };

  return {
    rois: [],
    selectedIds: [],
    hoveredId: null,
    nextNumber: 1,
    past: [],
    future: [],
    editSnapshot: null,
    activeShape: null,

    setActiveShape: (activeShape) => set({ activeShape }),

    addActiveRoi: () => {
      const { activeShape } = get();
      if (!activeShape) {
        return null;
      }
      const id = get().addRoi(activeShape);
      set({ activeShape: null });
      return id;
    },

    addRoi: (shape, name) => {
      const { rois, nextNumber } = get();
      const roi: ManagedRoi = { id: newRoiId(), name: name ?? `ROI ${nextNumber}`, color: roiColor(nextNumber - 1), visible: true, shape };
      commit([...rois, roi], { selectedIds: [roi.id], nextNumber: nextNumber + 1 });
      return roi.id;
    },

    importRois: (imported) => {
      const { rois, nextNumber } = get();
      // Ids already taken, including those chosen earlier in this import (a file may repeat an id)
      const existing = new Set(rois.map((roi) => roi.id));
      const added = imported.map((roi, i) => {
        const id = roi.id && !existing.has(roi.id) ? roi.id : newRoiId();
        existing.add(id);
        return { id, name: roi.name, color: roi.color || roiColor(nextNumber - 1 + i), visible: roi.visible ?? true, shape: roi.shape };
      });
      commit([...rois, ...added], { selectedIds: added.map((roi) => roi.id), nextNumber: nextNumber + added.length });
      return added.map((roi) => roi.id);
    },

    deleteRois: (ids) => {
      const remove = new Set(ids);
      const { rois, selectedIds } = get();
      const remaining = rois.filter((roi) => !remove.has(roi.id));
      if (remaining.length !== rois.length) {
        commit(remaining, { selectedIds: validSelection(remaining, selectedIds) });
      }
    },

    duplicateRois: (ids) => {
      const { rois, nextNumber } = get();
      const copies = rois
        .filter((roi) => ids.includes(roi.id))
        .map((roi, i) => ({
          ...roi,
          id: newRoiId(),
          name: `${roi.name} copy`,
          color: roiColor(nextNumber - 1 + i),
          shape: translateShape(roi.shape, DUPLICATE_OFFSET, DUPLICATE_OFFSET),
        }));
      if (copies.length > 0) {
        commit([...rois, ...copies], { selectedIds: copies.map((roi) => roi.id), nextNumber: nextNumber + copies.length });
      }
      return copies.map((roi) => roi.id);
    },

    renameRoi: (id, name) => {
      const trimmed = name.trim();
      const { rois } = get();
      if (trimmed && rois.some((roi) => roi.id === id && roi.name !== trimmed)) {
        commit(rois.map((roi) => (roi.id === id ? { ...roi, name: trimmed } : roi)));
      }
    },

    recolorRoi: (id, color) => {
      const { rois } = get();
      if (/^#[0-9A-Fa-f]{6}$/.test(color) && rois.some((roi) => roi.id === id && roi.color.toLowerCase() !== color.toLowerCase())) {
        commit(rois.map((roi) => (roi.id === id ? { ...roi, color } : roi)));
      }
    },

    replaceShape: (id, shape) => {
      commit(get().rois.map((roi) => (roi.id === id ? { ...roi, shape } : roi)));
    },

    nudgeRois: (ids, dx, dy) => {
      if (ids.length === 0) {
        return;
      }
      commit(get().rois.map((roi) => (ids.includes(roi.id) ? { ...roi, shape: translateShape(roi.shape, dx, dy) } : roi)));
    },

    // Visibility is a view setting, not an edit, so it is not undoable
    setVisible: (id, visible) => set({ rois: get().rois.map((roi) => (roi.id === id ? { ...roi, visible } : roi)) }),
    setAllVisible: (visible) => set({ rois: get().rois.map((roi) => ({ ...roi, visible })) }),

    beginEdit: () => set({ editSnapshot: get().rois }),

    updateShapeLive: (id, shape) => set({ rois: get().rois.map((roi) => (roi.id === id ? { ...roi, shape } : roi)) }),

    endEdit: () => {
      const { editSnapshot, rois, past } = get();
      if (editSnapshot && editSnapshot !== rois) {
        set({ past: [...past, editSnapshot].slice(-HISTORY_LIMIT), future: [], editSnapshot: null });
      } else {
        set({ editSnapshot: null });
      }
    },

    select: (ids) => set({ selectedIds: [...ids] }),

    toggleSelected: (id) => {
      const { selectedIds } = get();
      set({ selectedIds: selectedIds.includes(id) ? selectedIds.filter((other) => other !== id) : [...selectedIds, id] });
    },

    // Shift-click in the manager: from the last selected ROI to this one, in list order
    selectRange: (id) => {
      const { rois, selectedIds } = get();
      const anchor = selectedIds.at(-1);
      const from = rois.findIndex((roi) => roi.id === anchor);
      const to = rois.findIndex((roi) => roi.id === id);
      if (from < 0 || to < 0) {
        set({ selectedIds: [id] });
        return;
      }
      const [low, high] = from <= to ? [from, to] : [to, from];
      const range = rois.slice(low, high + 1).map((roi) => roi.id);
      set({ selectedIds: [...new Set([...selectedIds, ...range])] });
    },

    selectAll: () => set({ selectedIds: get().rois.map((roi) => roi.id) }),

    setHovered: (hoveredId) => {
      if (get().hoveredId !== hoveredId) {
        set({ hoveredId });
      }
    },

    undo: () => {
      const { past, future, rois, selectedIds } = get();
      const previous = past.at(-1);
      if (!previous) {
        return;
      }
      set({ rois: previous, past: past.slice(0, -1), future: [rois, ...future], selectedIds: validSelection(previous, selectedIds) });
    },

    redo: () => {
      const { past, future, rois, selectedIds } = get();
      const next = future[0];
      if (!next) {
        return;
      }
      set({ rois: next, past: [...past, rois], future: future.slice(1), selectedIds: validSelection(next, selectedIds) });
    },

    reset: () => set({ rois: [], selectedIds: [], hoveredId: null, nextNumber: 1, past: [], future: [], editSnapshot: null, activeShape: null }),
  };
});
