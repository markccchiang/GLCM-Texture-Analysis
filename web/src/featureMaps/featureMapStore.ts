// The feature map shown over the image: started on the server, polled until it finishes, then its values are fetched.
// One map at a time; it is not saved, and closing it or opening another image cancels or forgets it on the server.

import type { FeatureMapInfo, FeatureMapRequest } from '@glcm/api';
import { create } from 'zustand';
import { deleteFeatureMap, getFeatureMap, getFeatureMapValues, startFeatureMap } from '../api/client';
import type { ColorTableId } from '../image/colorTables';
import { useViewer } from '../stores/viewerStore';
import { finiteRange, percentileWindow, type ValueRange } from './mapImage';
import type { FeatureMapChoice } from './settings';

export const POLL_INTERVAL_MS = 300;

export interface FeatureMapView {
  info: FeatureMapInfo;
  /** rows × columns values once the map has completed */
  values: Float32Array | null;
  /** Finite minimum and maximum; null without values or when no value is finite */
  range: ValueRange | null;
  window: ValueRange | null;
  colorTable: ColorTableId;
  /** 0-1 */
  opacity: number;
  visible: boolean;
  /** Why the map failed, or why its status or values could not be read */
  error: string | null;
}

export interface FeatureMapState {
  map: FeatureMapView | null;
  /** The dialog's choices of the last started map */
  lastChoice: FeatureMapChoice | null;
  /** Starts a map, replacing the current one; resolves once the server accepted it and rejects when it did not */
  start(request: FeatureMapRequest, choice: FeatureMapChoice): Promise<void>;
  close(): void;
  setWindow(min: number, max: number): void;
  resetWindow(mode: 'auto' | 'full'): void;
  setColorTable(colorTable: ColorTableId): void;
  setOpacity(opacity: number): void;
  setVisible(visible: boolean): void;
}

/** Incremented whenever a map is started or closed, so that a stale poll stops */
let generation = 0;

const delay = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

const isRunning = (info: FeatureMapInfo) => info.status === 'queued' || info.status === 'running';

export const useFeatureMap = create<FeatureMapState>()((set, get) => {
  /** Applies a change to the map of this generation only */
  const update = (run: number, change: (map: FeatureMapView) => Partial<FeatureMapView>) => {
    const { map } = get();
    if (run === generation && map) {
      set({ map: { ...map, ...change(map) } });
    }
  };

  const follow = async (run: number, started: FeatureMapInfo) => {
    let info = started;
    try {
      while (isRunning(info)) {
        await delay(POLL_INTERVAL_MS);
        if (run !== generation) {
          return;
        }
        info = await getFeatureMap(info.featureMapId);
        const current = info;
        update(run, () => ({ info: current }));
      }
      if (info.status !== 'completed') {
        const reason = info.status === 'failed' ? (info.error ?? 'The feature map failed') : 'The feature map was cancelled';
        update(run, () => ({ error: reason }));
        return;
      }
      const values = await getFeatureMapValues(info);
      update(run, () => ({ values, range: finiteRange(values), window: percentileWindow(values) }));
    } catch (error) {
      update(run, () => ({ error: error instanceof Error ? error.message : String(error) }));
    }
  };

  return {
    map: null,
    lastChoice: null,

    start: async (request, choice) => {
      const previous = get().map;
      get().close();
      const run = generation;
      set({ lastChoice: choice });
      const info = await startFeatureMap(request);
      if (run !== generation) {
        void deleteFeatureMap(info.featureMapId).catch(() => undefined);
        return;
      }
      set({
        map: {
          info,
          values: null,
          range: null,
          window: null,
          colorTable: previous?.colorTable ?? 'viridis',
          opacity: previous?.opacity ?? 0.6,
          visible: true,
          error: null,
        },
      });
      void follow(run, info);
    },

    close: () => {
      generation += 1;
      const { map } = get();
      if (map) {
        set({ map: null });
        // Cancels a running map, or frees a finished one
        void deleteFeatureMap(map.info.featureMapId).catch(() => undefined);
      }
    },

    setWindow: (min, max) => update(generation, () => ({ window: min <= max ? { min, max } : { min: max, max: min } })),

    resetWindow: (mode) => update(generation, (map) => ({ window: map.values ? (mode === 'auto' ? percentileWindow(map.values) : map.range) : null })),

    setColorTable: (colorTable) => update(generation, () => ({ colorTable })),

    setOpacity: (opacity) => update(generation, () => ({ opacity: Math.min(1, Math.max(0, opacity)) })),

    setVisible: (visible) => update(generation, () => ({ visible })),
  };
});

// A map belongs to one image
useViewer.subscribe((state, previous) => {
  if (state.image?.info.imageId !== previous.image?.info.imageId) {
    useFeatureMap.getState().close();
  }
});
