import { ActionIcon, Button, Group, NumberInput, Popover, RangeSlider, Stack, Text } from '@mantine/core';
import { IconAdjustmentsHorizontal } from '@tabler/icons-react';
import { histogramPath, valueToHistogramX } from '../image/histogram';
import { useUi } from '../stores/uiStore';
import { maxSampleValue, useViewer } from '../stores/viewerStore';

const HISTOGRAM_WIDTH = 256;
const HISTOGRAM_HEIGHT = 72;

function WindowLevelPanel() {
  const image = useViewer((state) => state.image);
  const window = useViewer((state) => state.window);
  const viewer = useViewer.getState;
  if (!image) {
    return null;
  }
  const maxValue = maxSampleValue(image.info.bitDepth);
  const minX = valueToHistogramX(window.min, maxValue, HISTOGRAM_WIDTH);
  const maxX = valueToHistogramX(window.max + 1, maxValue, HISTOGRAM_WIDTH);

  return (
    <Stack gap="xs" w={HISTOGRAM_WIDTH}>
      <Text size="xs" c="dimmed">
        Histogram (log scale), {image.info.bitDepth}-bit
      </Text>
      <svg className="histogram" width={HISTOGRAM_WIDTH} height={HISTOGRAM_HEIGHT} role="img" aria-label="Intensity histogram">
        <rect x={minX} y={0} width={Math.max(1, maxX - minX)} height={HISTOGRAM_HEIGHT} fill="rgba(34, 139, 230, 0.2)" />
        <path d={histogramPath(image.info.histogram, HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT)} fill="#adb5bd" />
        <line x1={minX} x2={minX} y1={0} y2={HISTOGRAM_HEIGHT} stroke="#228be6" />
        <line x1={maxX} x2={maxX} y1={0} y2={HISTOGRAM_HEIGHT} stroke="#228be6" />
      </svg>
      <Group gap="xs" grow>
        <NumberInput
          size="xs"
          label="Min"
          min={0}
          max={maxValue}
          allowDecimal={false}
          value={window.min}
          onChange={(value) => typeof value === 'number' && viewer().setWindow(value, Math.max(value, window.max))}
        />
        <NumberInput
          size="xs"
          label="Max"
          min={0}
          max={maxValue}
          allowDecimal={false}
          value={window.max}
          onChange={(value) => typeof value === 'number' && viewer().setWindow(Math.min(value, window.min), value)}
        />
      </Group>
      <Group gap="xs" grow>
        <Button size="compact-xs" variant="light" onClick={() => viewer().resetWindow('auto')}>
          Auto
        </Button>
        <Button size="compact-xs" variant="light" onClick={() => viewer().resetWindow('full')}>
          Full range
        </Button>
      </Group>
      <Text size="xs" c="dimmed">
        Auto = 0.5–99.5 percentiles ({image.info.windowMin}–{image.info.windowMax})
      </Text>
    </Stack>
  );
}

export function WindowLevelControl() {
  const image = useViewer((state) => state.image);
  const window = useViewer((state) => state.window);
  const panelOpen = useUi((state) => state.windowPanelOpen);
  const maxValue = image ? maxSampleValue(image.info.bitDepth) : 255;

  return (
    <Group gap={6} wrap="nowrap">
      <Text size="xs" c="dimmed">
        Window
      </Text>
      <Text size="xs" className="mono" w={40} ta="right">
        {image ? window.min : '–'}
      </Text>
      <RangeSlider
        w={180}
        size="sm"
        min={0}
        max={maxValue}
        minRange={0}
        label={null}
        disabled={!image}
        value={[window.min, window.max]}
        onChange={([min, max]) => useViewer.getState().setWindow(min, max)}
        thumbFromLabel="Window minimum"
        thumbToLabel="Window maximum"
      />
      <Text size="xs" className="mono" w={40}>
        {image ? window.max : '–'}
      </Text>
      <Popover opened={panelOpen && image !== null} onChange={(open) => useUi.getState().setWindowPanelOpen(open)} position="bottom" shadow="md" withArrow>
        <Popover.Target>
          <ActionIcon variant="subtle" color="gray" disabled={!image} aria-label="Window/level settings" onClick={() => useUi.getState().setWindowPanelOpen(!panelOpen)}>
            <IconAdjustmentsHorizontal size={18} />
          </ActionIcon>
        </Popover.Target>
        <Popover.Dropdown>
          <WindowLevelPanel />
        </Popover.Dropdown>
      </Popover>
    </Group>
  );
}
