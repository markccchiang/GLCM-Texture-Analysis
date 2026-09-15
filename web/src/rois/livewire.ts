// Livewire drawing: anchors clicked on pixels, joined by paths along strong edges that the server computes.

export type LivewireSegment = Array<[number, number]>;

/** Centre of the pixel under an image point, where livewire paths start and end */
export function pixelCentre(pixel: { x: number; y: number }): [number, number] {
  return [pixel.x + 0.5, pixel.y + 0.5];
}

/**
 * One outline from consecutive livewire segments, each starting where the previous one ended. The shared end points are
 * kept once; for a closed outline the last point, which repeats the first anchor, is dropped too.
 */
export function joinSegments(segments: readonly LivewireSegment[], closed: boolean): Array<[number, number]> {
  const points: Array<[number, number]> = [];
  for (const segment of segments) {
    for (const [x, y] of segment) {
      const last = points.at(-1);
      if (!last || last[0] !== x || last[1] !== y) {
        points.push([x, y]);
      }
    }
  }
  if (closed && points.length > 1) {
    const [firstX, firstY] = points[0];
    const [lastX, lastY] = points[points.length - 1];
    if (firstX === lastX && firstY === lastY) {
      points.pop();
    }
  }
  return points;
}
