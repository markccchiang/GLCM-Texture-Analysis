// The ruler over the image: the line at a constant screen width with its end points and a label with the length and
// angle. It does not take pointer input; ImageCanvas draws it with the ruler tool.

import { Circle, Label, Layer, Line, Tag, Text } from 'react-konva';
import { useViewer } from '../stores/viewerStore';
import { formatRuler, measureRuler } from './ruler';
import { imageToScreen, type Viewport } from './viewport';

const HALO = 'rgba(0, 0, 0, 0.75)';

export function RulerLayer({ viewport }: { viewport: Viewport }) {
  const ruler = useViewer((state) => state.ruler);
  const spacing = useViewer((state) => state.pixelSpacing);
  if (!ruler) {
    return null;
  }
  const start = imageToScreen(viewport, ruler.start);
  const end = imageToScreen(viewport, ruler.end);
  const points = [start.x, start.y, end.x, end.y];
  return (
    <Layer listening={false}>
      <Line points={points} stroke={HALO} strokeWidth={3.5} lineCap="round" />
      <Line points={points} stroke="#FFFFFF" strokeWidth={1.5} lineCap="round" />
      {[start, end].map((point, index) => (
        <Circle key={index} x={point.x} y={point.y} radius={3.5} fill="#FFFFFF" stroke={HALO} strokeWidth={1.5} />
      ))}
      <Label x={end.x + 10} y={end.y + 10}>
        <Tag fill={HALO} cornerRadius={3} />
        <Text text={formatRuler(measureRuler(ruler, spacing))} fontSize={12} fontFamily="ui-monospace, SFMono-Regular, Menlo, monospace" fill="#FFFFFF" padding={4} />
      </Label>
    </Layer>
  );
}
