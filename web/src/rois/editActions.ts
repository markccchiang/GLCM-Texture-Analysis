// Brush, eraser, union and subtract: the server computes the result on the pixel grid and the ROIs are updated with it.

import { MAX_POLYGON_VERTICES, type PolygonShape, type RoiOperation, type RoiShape } from '@glcm/api';
import { notifications } from '@mantine/notifications';
import { brushRoi, combineRois } from '../api/client';
import { useViewer } from '../stores/viewerStore';
import { simplifyPolyline } from './geometry';
import { regionShape } from './regions';
import { useRois } from './roiStore';

function notify(title: string, message: string, color = 'yellow'): void {
  notifications.show({ color, title, message });
}

function notifySimplified(simplified: boolean): void {
  if (simplified) {
    notify(
      'Outline simplified',
      `An ROI may have at most ${MAX_POLYGON_VERTICES.toLocaleString()} vertices, so the outline was simplified and its edges no longer follow the pixels exactly.`,
    );
  }
}

/** Strokes run one after another, so each one starts from the result of the previous stroke */
let strokes: Promise<void> = Promise.resolve();

/** A stroke path that fits into a request */
export function strokePath(path: ReadonlyArray<readonly [number, number]>, maxPoints = MAX_POLYGON_VERTICES): Array<[number, number]> {
  let points = path.map(([x, y]): [number, number] => [x, y]);
  for (let tolerance = 0.25; points.length > maxPoints; tolerance *= 2) {
    points = simplifyPolyline(path, tolerance);
  }
  return points;
}

/**
 * Paints (or erases) a stroke into the one selected ROI; painting without a selection creates a new ROI, which is then
 * selected, so further strokes add to it. Each stroke is one undo step.
 */
export function applyBrushStroke(path: ReadonlyArray<readonly [number, number]>, erase: boolean): Promise<void> {
  const next = strokes.then(() => runBrushStroke(path, erase));
  strokes = next.catch(() => undefined);
  return next;
}

async function runBrushStroke(path: ReadonlyArray<readonly [number, number]>, erase: boolean): Promise<void> {
  const image = useViewer.getState().image;
  if (!image || path.length === 0) {
    return;
  }
  const { rois, selectedIds } = useRois.getState();
  if (selectedIds.length > 1) {
    notify(erase ? 'Select one ROI to erase from' : 'Select one ROI to paint into', 'The brush and the eraser change a single selected ROI; with no ROI selected, the brush starts a new one.');
    return;
  }
  const target = selectedIds.length === 1 ? rois.find((roi) => roi.id === selectedIds[0]) : undefined;
  if (erase && !target) {
    notify('Select an ROI to erase from', 'The eraser removes pixels from the selected ROI.');
    return;
  }
  try {
    const result = await brushRoi(image.info.imageId, {
      shape: target?.shape ?? null,
      path: strokePath(path),
      radius: useViewer.getState().brushSize / 2,
      erase,
    });
    if (useViewer.getState().image?.info.imageId === image.info.imageId) {
      applyResult(target ? { id: target.id, shape: target.shape } : null, result.shape);
    }
  } catch (error) {
    notify(erase ? 'Eraser failed' : 'Brush failed', (error as Error).message, 'red');
  }
}

/** Replaces the target's shape (or adds a new ROI); the target is left alone if it changed while the request ran */
function applyResult(target: { id: string; shape: RoiShape } | null, result: PolygonShape | null): void {
  const store = useRois.getState();
  const current = target ? store.rois.find((roi) => roi.id === target.id) : undefined;
  if (target && current?.shape !== target.shape) {
    return;
  }
  if (!result) {
    if (current) {
      store.deleteRois([current.id]);
      notify('ROI removed', `Nothing was left of ${current.name}, so it was deleted. Undo brings it back.`);
    }
    return;
  }
  const { shape, simplified } = regionShape(result);
  if (current) {
    store.replaceShape(current.id, shape);
  } else {
    store.addRoi(shape);
  }
  notifySimplified(simplified);
}

/**
 * Union: the selected ROIs become one, which keeps the first selected ROI's name and colour. Subtract: the other selected
 * ROIs are removed from the first selected ROI, and stay. Both are one undo step.
 */
export async function combineSelectedRois(operation: RoiOperation): Promise<void> {
  const image = useViewer.getState().image;
  if (!image) {
    return;
  }
  const { rois, selectedIds } = useRois.getState();
  const selected = selectedIds.flatMap((id) => rois.filter((roi) => roi.id === id));
  if (selected.length < 2) {
    notify('Select at least two ROIs', operation === 'union' ? 'Union merges the selected ROIs into one.' : 'Subtract removes the other selected ROIs from the first one you selected.');
    return;
  }
  const [first, ...others] = selected;
  try {
    const result = await combineRois(image.info.imageId, { operation, shapes: selected.map((roi) => roi.shape) });
    const store = useRois.getState();
    const unchanged = selected.every((roi) => store.rois.find((candidate) => candidate.id === roi.id)?.shape === roi.shape);
    if (useViewer.getState().image?.info.imageId !== image.info.imageId || !unchanged) {
      return;
    }
    if (!result.shape) {
      notify(
        operation === 'union' ? 'Nothing to combine' : 'Nothing would be left',
        operation === 'union' ? 'The selected ROIs cover no pixel of the image.' : `The other ROIs cover all of ${first.name}, so it was not changed.`,
      );
      return;
    }
    const { shape, simplified } = regionShape(result.shape);
    if (operation === 'union') {
      store.mergeRois(first.id, shape, others.map((roi) => roi.id));
    } else {
      store.replaceShape(first.id, shape);
      store.select([first.id]);
    }
    notifySimplified(simplified);
  } catch (error) {
    notify(operation === 'union' ? 'Union failed' : 'Subtract failed', (error as Error).message, 'red');
  }
}
