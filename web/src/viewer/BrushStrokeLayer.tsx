// The brush or eraser stroke being drawn, at its real width, until the server returns the changed ROI

import { Group, Layer, Line } from 'react-konva';
import type { Viewport } from './viewport';

export interface BrushStroke {
  path: Array<[number, number]>;
  erase: boolean;
}

export function BrushStrokeLayer({ viewport, stroke, size }: { viewport: Viewport; stroke: BrushStroke; size: number }) {
  const [first] = stroke.path;
  // A single point is drawn as a zero-length line, which the round cap turns into a disc
  const points = stroke.path.length === 1 ? [first[0], first[1], first[0], first[1]] : stroke.path.flat();
  return (
    <Layer listening={false}>
      <Group x={viewport.x} y={viewport.y} scaleX={viewport.scale} scaleY={viewport.scale}>
        <Line
          points={points}
          stroke={stroke.erase ? 'rgba(255, 82, 82, 0.55)' : 'rgba(255, 255, 255, 0.45)'}
          strokeWidth={size}
          lineCap="round"
          lineJoin="round"
          listening={false}
          perfectDrawEnabled={false}
        />
      </Group>
    </Layer>
  );
}
