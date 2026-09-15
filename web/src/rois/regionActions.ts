// The magic wand and Threshold ROI flows: ask the server for regions and turn them into ROIs.

import { MAX_POLYGON_VERTICES } from '@glcm/api';
import { notifications } from '@mantine/notifications';
import { selectThresholdRois, selectWandRoi } from '../api/client';
import { useViewer } from '../stores/viewerStore';
import { regionShape } from './regions';
import { useRois } from './roiStore';

function notifySimplified(count: number): void {
  if (count > 0) {
    notifications.show({
      color: 'yellow',
      title: count === 1 ? 'Outline simplified' : `${count} outlines simplified`,
      message: `An ROI may have at most ${MAX_POLYGON_VERTICES.toLocaleString()} vertices, so longer outlines were simplified and their edges no longer follow the pixels exactly.`,
    });
  }
}

/** Makes the region around an image pixel the active ROI */
export async function wandAt(imageId: string, x: number, y: number): Promise<void> {
  const tolerance = useViewer.getState().wandTolerance;
  try {
    const { region } = await selectWandRoi(imageId, { x, y, tolerance });
    // The image may have changed while the request ran
    if (!region || useViewer.getState().image?.info.imageId !== imageId) {
      return;
    }
    const { shape, simplified } = regionShape(region);
    useRois.getState().setActiveShape(shape);
    notifySimplified(simplified ? 1 : 0);
  } catch (error) {
    notifications.show({ color: 'red', title: 'Magic wand failed', message: (error as Error).message });
  }
}

/** Adds the largest regions inside the display window to the ROI Manager; resolves to the number added */
export async function addThresholdRois(imageId: string, minPixels: number, count: number): Promise<number> {
  const { min, max } = useViewer.getState().window;
  const { regions } = await selectThresholdRois(imageId, { min, max, minPixels, maxRegions: count });
  if (useViewer.getState().image?.info.imageId !== imageId || regions.length === 0) {
    return 0;
  }
  const rois = useRois.getState();
  const shapes = regions.map((region) => regionShape(region));
  rois.importRois(shapes.map(({ shape }, i) => ({ name: `ROI ${rois.nextNumber + i}`, color: '', shape })));
  notifySimplified(shapes.filter(({ simplified }) => simplified).length);
  return regions.length;
}
