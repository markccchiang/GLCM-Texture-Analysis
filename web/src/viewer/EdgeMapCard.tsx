// Card over the canvas for the edge map: method, smoothing, limits and opacity

import { ActionIcon, Button, Group, NumberInput, SegmentedControl, Slider, Stack, Text } from '@mantine/core';
import { IconX } from '@tabler/icons-react';
import { useViewer } from '../stores/viewerStore';
import { autoEdgeLimits, roundLimit, useEdgeMap } from './edgeMap';
import { useGradientStats } from './EdgeMapLayer';

export function EdgeMapCard() {
  const imageId = useViewer((state) => state.image?.info.imageId ?? null);
  const shown = useEdgeMap((state) => state.shown);
  const method = useEdgeMap((state) => state.method);
  const sigma = useEdgeMap((state) => state.sigma);
  const chosen = useEdgeMap((state) => state.limits);
  const opacity = useEdgeMap((state) => state.opacity);
  const statistics = useGradientStats(imageId, sigma, shown);
  if (!shown || !imageId) {
    return null;
  }
  const store = useEdgeMap.getState;
  const limits = chosen ?? (statistics.data ? autoEdgeLimits(method, statistics.data) : null);
  const [lowLabel, highLabel] = method === 'sobel' ? ['Black at', 'White at'] : ['Low threshold', 'High threshold'];

  return (
    <section className="edge-map-card" aria-label="Edge map" data-testid="edge-map-card">
      <Group justify="space-between" wrap="nowrap" gap="xs">
        <Text size="sm" fw={600}>
          Edge map
        </Text>
        <ActionIcon size="sm" variant="subtle" color="gray" aria-label="Hide edge map" onClick={() => store().setShown(false)}>
          <IconX size={14} />
        </ActionIcon>
      </Group>
      <Stack gap={6}>
        <SegmentedControl
          size="xs"
          fullWidth
          aria-label="Edge detection method"
          data={[
            { value: 'canny', label: 'Canny' },
            { value: 'sobel', label: 'Sobel' },
          ]}
          value={method}
          onChange={(value) => store().setMethod(value as 'canny' | 'sobel')}
        />
        <NumberInput
          size="xs"
          label="Smoothing σ (pixels)"
          description="Also used by the livewire tool"
          min={0}
          max={10}
          step={0.5}
          decimalScale={2}
          value={sigma}
          onChange={(value) => typeof value === 'number' && store().setSigma(value)}
        />
        <Group gap="xs" grow wrap="nowrap">
          <NumberInput
            size="xs"
            label={lowLabel}
            min={0}
            decimalScale={4}
            value={limits ? roundLimit(limits.low) : ''}
            disabled={!limits}
            onChange={(value) => typeof value === 'number' && limits && store().setLimits({ low: value, high: limits.high })}
          />
          <NumberInput
            size="xs"
            label={highLabel}
            min={0}
            decimalScale={4}
            value={limits ? roundLimit(limits.high) : ''}
            disabled={!limits}
            onChange={(value) => typeof value === 'number' && limits && store().setLimits({ low: limits.low, high: value })}
          />
        </Group>
        <Button size="compact-xs" variant="light" disabled={chosen === null} onClick={() => store().setLimits(null)}>
          Auto
        </Button>
        <Text size="xs" c="dimmed">
          {statistics.data
            ? `Gradient per pixel: median ${roundLimit(statistics.data.percentiles['50'])}, 95th percentile ${roundLimit(statistics.data.percentiles['95'])}, maximum ${roundLimit(statistics.data.max)}`
            : 'Measuring the gradient…'}
        </Text>
        <div>
          <Text size="xs" mb={2}>
            Opacity
          </Text>
          <Slider
            size="sm"
            min={0}
            max={100}
            label={(value) => `${value} %`}
            value={Math.round(opacity * 100)}
            onChange={(value) => store().setOpacity(value / 100)}
            thumbProps={{ 'aria-label': 'Edge map opacity' }}
          />
        </div>
      </Stack>
    </section>
  );
}
