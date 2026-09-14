import { Text } from '@mantine/core';
import { useViewer, type RendererKind } from '../stores/viewerStore';

const RENDERER_LABELS: Record<RendererKind, string> = {
  webgl2: 'WebGL2',
  lut: 'Lookup table',
  server: 'Server rendering',
};

export function StatusBar() {
  const hover = useViewer((state) => state.hover);
  const image = useViewer((state) => state.image);
  const scale = useViewer((state) => state.viewport.scale);
  const rendererKind = useViewer((state) => state.rendererKind);
  const info = image?.info;

  return (
    <footer className="status-bar" data-testid="status-bar">
      <Text size="xs" className="mono" w={230} data-testid="pixel-readout">
        {hover ? `x ${hover.x}  y ${hover.y}  value ${hover.value ?? '…'}` : 'x –  y –  value –'}
      </Text>
      <Text size="xs" className="mono" w={80}>
        {info ? `zoom ${Math.round(scale * 100)}%` : ''}
      </Text>
      {info && (
        <Text size="xs" truncate>
          {info.name} {info.width}×{info.height} {info.bitDepth}-bit
          {info.sourceChannels > 1 ? ' (converted to grayscale)' : ''}
        </Text>
      )}
      {rendererKind && (
        <Text size="xs" c="dimmed" ml="auto">
          {RENDERER_LABELS[rendererKind]}
        </Text>
      )}
    </footer>
  );
}
