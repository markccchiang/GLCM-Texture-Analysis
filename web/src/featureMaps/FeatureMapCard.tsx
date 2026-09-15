// Card over the canvas for the feature map: progress while it is computed, then its window, colour table, opacity, the
// value under the pointer and saving.

import { ActionIcon, Button, Group, NumberInput, Progress, Select, Slider, Stack, Switch, Text } from '@mantine/core';
import { notifications } from '@mantine/notifications';
import { IconX } from '@tabler/icons-react';
import { useQuery } from '@tanstack/react-query';
import { CATALOG_QUERY } from '../api/queryClient';
import { COLOR_TABLES, colorTableById, cssGradient, type ColorTableId } from '../image/colorTables';
import { useViewer } from '../stores/viewerStore';
import { saveFeatureMapPng, saveFeatureMapTiff } from './exportMap';
import { useFeatureMap, type FeatureMapView } from './featureMapStore';
import { formatMapValue, valueAt } from './mapImage';

function PointerValue({ map }: { map: FeatureMapView }) {
  const hover = useViewer((state) => state.hover);
  if (!hover || !map.values) {
    return null;
  }
  const value = valueAt(map.info, map.values, hover.x, hover.y);
  if (value === null) {
    return null;
  }
  return (
    <Text size="xs" className="mono" data-testid="feature-map-value">
      ({hover.x}, {hover.y}): {formatMapValue(value)}
    </Text>
  );
}

function MapControls({ map }: { map: FeatureMapView }) {
  const store = useFeatureMap.getState;
  const window = map.window!;
  const range = map.range!;
  // Enough decimals for four significant digits of the value range
  const span = Math.max(Math.abs(range.max - range.min), Math.abs(range.max), Number.MIN_VALUE);
  const decimals = Math.min(12, Math.max(0, 3 - Math.floor(Math.log10(span))));
  const save = async (format: 'png' | 'tiff') => {
    try {
      if (format === 'png') {
        await saveFeatureMapPng(map);
      } else {
        saveFeatureMapTiff(map);
      }
    } catch (error) {
      notifications.show({ color: 'red', title: 'Could not save the feature map', message: (error as Error).message });
    }
  };

  return (
    <Stack gap={6}>
      <div className="feature-map-colour-bar" style={{ background: cssGradient(colorTableById(map.colorTable)) }} data-testid="feature-map-colours" />
      <Group gap="xs" grow wrap="nowrap">
        <NumberInput size="xs" label="Min" decimalScale={decimals} value={window.min} onChange={(value) => typeof value === 'number' && store().setWindow(value, window.max)} />
        <NumberInput size="xs" label="Max" decimalScale={decimals} value={window.max} onChange={(value) => typeof value === 'number' && store().setWindow(window.min, value)} />
      </Group>
      <Group gap="xs" grow>
        <Button size="compact-xs" variant="light" onClick={() => store().resetWindow('auto')}>
          Auto
        </Button>
        <Button size="compact-xs" variant="light" onClick={() => store().resetWindow('full')}>
          Full range
        </Button>
      </Group>
      <Text size="xs" c="dimmed">
        Values {formatMapValue(range.min)} to {formatMapValue(range.max)}; Auto = 0.5–99.5 percentiles
      </Text>
      <Select
        size="xs"
        label="Colour table"
        allowDeselect={false}
        data={COLOR_TABLES.map((table) => ({ value: table.id, label: table.name }))}
        value={map.colorTable}
        onChange={(value) => value && store().setColorTable(value as ColorTableId)}
        comboboxProps={{ withinPortal: false }}
      />
      <div>
        <Text size="xs" mb={2}>
          Opacity
        </Text>
        <Slider
          size="sm"
          min={0}
          max={100}
          label={(value) => `${value} %`}
          value={Math.round(map.opacity * 100)}
          onChange={(value) => store().setOpacity(value / 100)}
          thumbProps={{ 'aria-label': 'Feature map opacity' }}
        />
      </div>
      <Switch size="xs" label="Show over the image" checked={map.visible} onChange={(event) => store().setVisible(event.currentTarget.checked)} />
      <PointerValue map={map} />
      <Group gap="xs" grow>
        <Button size="compact-xs" variant="default" onClick={() => void save('png')}>
          Save PNG
        </Button>
        <Button size="compact-xs" variant="default" onClick={() => void save('tiff')}>
          Save TIFF
        </Button>
      </Group>
    </Stack>
  );
}

export function FeatureMapCard() {
  const map = useFeatureMap((state) => state.map);
  const catalog = useQuery(CATALOG_QUERY);
  if (!map) {
    return null;
  }
  const { info } = map;
  const running = info.status === 'queued' || info.status === 'running';
  const name = catalog.data?.features.find((feature) => feature.id === info.settings.feature)?.name ?? info.settings.feature;
  const percent = info.rows > 0 ? Math.round((info.completedRows / info.rows) * 100) : 0;

  let body;
  if (map.error) {
    body = (
      <Text size="xs" c="red">
        {map.error}
      </Text>
    );
  } else if (running) {
    body = (
      <Stack gap={4}>
        <Progress value={info.status === 'queued' ? 100 : percent} animated={info.status === 'queued'} aria-label="Feature map progress" />
        <Text size="xs" c="dimmed">
          {info.status === 'queued' ? 'Waiting for a worker' : `Computing — ${percent} %`}
        </Text>
      </Stack>
    );
  } else if (!map.values) {
    body = (
      <Text size="xs" c="dimmed">
        Loading values…
      </Text>
    );
  } else if (!map.range || !map.window) {
    body = (
      <Text size="xs" c="yellow">
        No window has pixel pairs at this distance in every selected direction.
      </Text>
    );
  } else {
    body = <MapControls map={map} />;
  }

  return (
    <section className="feature-map-card" aria-label="Feature map" data-testid="feature-map-card" data-status={info.status}>
      <Group justify="space-between" wrap="nowrap" gap="xs">
        <Text size="sm" fw={600} truncate>
          {name}
        </Text>
        <ActionIcon size="sm" variant="subtle" color="gray" aria-label={running ? 'Cancel feature map' : 'Close feature map'} onClick={() => useFeatureMap.getState().close()}>
          <IconX size={14} />
        </ActionIcon>
      </Group>
      <Text size="xs" c="dimmed">
        {info.columns}×{info.rows} · window {info.settings.window} px · step {info.step} · d {info.settings.distance}
      </Text>
      {body}
    </section>
  );
}
