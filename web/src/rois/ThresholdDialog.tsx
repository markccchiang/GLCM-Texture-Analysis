// ROI ▸ Threshold ROI…: every connected part of the pixels inside the display window becomes an ROI

import { MAX_ROIS_PER_REQUEST } from '@glcm/api';
import { Button, Group, NumberInput, Stack, Text } from '@mantine/core';
import { useDebouncedValue } from '@mantine/hooks';
import { notifications } from '@mantine/notifications';
import { useQuery } from '@tanstack/react-query';
import { useState } from 'react';
import { selectThresholdRois } from '../api/client';
import { useViewer } from '../stores/viewerStore';
import { addThresholdRois } from './regionActions';
import { DEFAULT_THRESHOLD_MIN_PIXELS } from './regions';

export function ThresholdRoiContent({ onClose }: { onClose(): void }) {
  const image = useViewer((state) => state.image);
  const window = useViewer((state) => state.window);
  const [minPixels, setMinPixels] = useState<number | string>(DEFAULT_THRESHOLD_MIN_PIXELS);
  const [debouncedMinPixels] = useDebouncedValue(minPixels, 250);
  const [adding, setAdding] = useState(false);
  const imageId = image?.info.imageId ?? null;
  const size = typeof debouncedMinPixels === 'number' && Number.isInteger(debouncedMinPixels) && debouncedMinPixels >= 1 ? debouncedMinPixels : null;

  const count = useQuery({
    queryKey: ['threshold-rois', imageId, window.min, window.max, size],
    queryFn: ({ signal }) => selectThresholdRois(imageId!, { min: window.min, max: window.max, minPixels: size!, maxRegions: 0 }, signal),
    enabled: imageId !== null && size !== null,
    staleTime: Infinity,
  });

  if (!image || !imageId) {
    return (
      <Text size="sm" c="dimmed">
        Open an image first.
      </Text>
    );
  }

  const total = count.data?.total;
  const adds = total === undefined ? 0 : Math.min(total, MAX_ROIS_PER_REQUEST);
  let summary = 'Enter a whole number of pixels.';
  if (size !== null) {
    if (count.isError) {
      summary = (count.error as Error).message;
    } else if (total === undefined) {
      summary = 'Counting regions…';
    } else {
      summary = `${total.toLocaleString()} ${total === 1 ? 'region' : 'regions'} of at least ${size.toLocaleString()} pixels.`;
      if (total > MAX_ROIS_PER_REQUEST) {
        summary += ` Only the largest ${MAX_ROIS_PER_REQUEST.toLocaleString()} are added; raise the minimum size to add fewer.`;
      }
    }
  }

  const add = async () => {
    setAdding(true);
    try {
      await addThresholdRois(imageId, size!, adds);
      onClose();
    } catch (error) {
      notifications.show({ color: 'red', title: 'Could not add the ROIs', message: (error as Error).message });
    } finally {
      setAdding(false);
    }
  };

  return (
    <Stack gap="sm">
      <Text size="sm">
        Selects the pixels from <strong className="mono">{window.min}</strong> to <strong className="mono">{window.max}</strong>, the current display
        window. Each connected part (pixels touching at an edge or a corner) becomes a polygon ROI, with its holes filled.
      </Text>
      <Text size="xs" c="dimmed">
        To select other intensities, close this dialog and change the window first.
      </Text>
      <NumberInput label="Minimum size" description="Pixels, holes included; smaller parts are left out" min={1} allowDecimal={false} value={minPixels} onChange={setMinPixels} />
      <Text size="sm" data-testid="threshold-summary">
        {summary}
      </Text>
      <Group justify="flex-end">
        <Button variant="default" onClick={onClose}>
          Cancel
        </Button>
        <Button loading={adding} disabled={adds === 0 || size === null || count.isFetching} onClick={() => void add()}>
          {adds === 1 ? 'Add 1 ROI' : `Add ${adds.toLocaleString()} ROIs`}
        </Button>
      </Group>
    </Stack>
  );
}
