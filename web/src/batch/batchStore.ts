// The batch in progress, kept outside the dialog: closing the dialog does not stop the batch, and reopening it shows the
// progress

import { create } from 'zustand';
import { browserBatchDependencies } from './dependencies';
import { runBatch, type BatchInput, type BatchItem } from './runBatch';

let controller: AbortController | null = null;

export interface BatchState {
  items: BatchItem[];
  running: boolean;
  start(input: BatchInput): Promise<void>;
  cancel(): void;
  clear(): void;
}

export const useBatch = create<BatchState>()((set, get) => ({
  items: [],
  running: false,
  start: async (input) => {
    if (get().running) {
      return;
    }
    controller = new AbortController();
    set({ running: true, items: [] });
    try {
      await runBatch(input, browserBatchDependencies(), (items) => set({ items }), controller.signal);
    } finally {
      controller = null;
      set({ running: false });
    }
  },
  cancel: () => controller?.abort(),
  clear: () => {
    if (!get().running) {
      set({ items: [] });
    }
  },
}));
