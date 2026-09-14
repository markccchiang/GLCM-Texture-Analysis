import { Badge, Kbd, List, Modal, NavLink, ScrollArea, SegmentedControl, Stack, Switch, Table, Text } from '@mantine/core';
import type { HealthResponse, SampleInfo } from '@glcm/api';
import { API_PREFIX } from '@glcm/api';
import { useQuery } from '@tanstack/react-query';
import type { ReactNode } from 'react';
import { getSamples } from '../api/client';
import { openSample } from '../stores/imageLoader';
import { usePreferences } from '../stores/preferences';
import { MOD_KEY, useUi, type ModalName } from '../stores/uiStore';
import { useViewer } from '../stores/viewerStore';
import type { ScrollBehaviour } from '../viewer/wheel';

function formatBytes(bytes: number): string {
  if (bytes < 1024) {
    return `${bytes} B`;
  }
  const units = ['KiB', 'MiB', 'GiB'];
  let value = bytes / 1024;
  let unit = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit += 1;
  }
  return `${value.toFixed(1)} ${units[unit]}`;
}

function InfoRows({ rows }: { rows: [string, ReactNode][] }) {
  return (
    <Table withRowBorders={false} verticalSpacing={4}>
      <Table.Tbody>
        {rows.map(([label, value]) => (
          <Table.Tr key={label}>
            <Table.Td w={140}>
              <Text size="sm" c="dimmed">
                {label}
              </Text>
            </Table.Td>
            <Table.Td>
              <Text size="sm" style={{ wordBreak: 'break-all' }}>
                {value}
              </Text>
            </Table.Td>
          </Table.Tr>
        ))}
      </Table.Tbody>
    </Table>
  );
}

function ImageInfoContent() {
  const image = useViewer((state) => state.image);
  const rendererKind = useViewer((state) => state.rendererKind);
  if (!image) {
    return <Text c="dimmed">No image is open.</Text>;
  }
  const { info } = image;
  return (
    <Stack gap="sm">
      <InfoRows
        rows={[
          ['Name', info.name],
          ['File size', formatBytes(info.sizeBytes)],
          ['Dimensions', `${info.width} × ${info.height} px`],
          ['Bit depth', `${info.bitDepth}-bit`],
          ['Channels', info.sourceChannels > 1 ? `${info.sourceChannels} (converted to grayscale)` : '1 (grayscale)'],
          ['Default window', `${info.windowMin} – ${info.windowMax}`],
          ['Pixel transfer', info.transfer === 'raw' ? 'Raw samples, rendered in the browser' : 'Server-rendered display.png'],
          ['Renderer', rendererKind ?? '–'],
          ['SHA-256', <span className="mono">{info.sha256}</span>],
          ['Image id', <span className="mono">{info.imageId}</span>],
          ['Uploaded', new Date(info.createdAt).toLocaleString()],
        ]}
      />
      {info.warnings.length > 0 && (
        <List size="sm" c="yellow">
          {info.warnings.map((warning) => (
            <List.Item key={warning}>{warning}</List.Item>
          ))}
        </List>
      )}
    </Stack>
  );
}

function PreferencesContent() {
  const scrollBehaviour = usePreferences((state) => state.scrollBehaviour);
  const useWebGl = usePreferences((state) => state.useWebGl);
  const preferences = usePreferences.getState;
  return (
    <Stack gap="lg">
      <Stack gap={6}>
        <Text size="sm" fw={500}>
          Scroll behaviour
        </Text>
        <SegmentedControl
          value={scrollBehaviour}
          onChange={(value) => preferences().setScrollBehaviour(value as ScrollBehaviour)}
          data={[
            { value: 'auto', label: 'Auto' },
            { value: 'zoom', label: 'Always zoom' },
            { value: 'pan', label: 'Always pan' },
          ]}
        />
        <Text size="xs" c="dimmed">
          Auto: a mouse wheel zooms and a trackpad two-finger scroll pans. Pinch and {MOD_KEY === '⌘' ? '⌘' : 'Ctrl'} + scroll always zoom.
        </Text>
      </Stack>
      <Switch
        checked={useWebGl}
        onChange={(event) => preferences().setUseWebGl(event.currentTarget.checked)}
        label="Render with WebGL2"
        description="Turn off to use the slower lookup-table renderer. Both give identical pixels."
      />
    </Stack>
  );
}

const SHORTCUTS: [string, string][] = [
  [`${MOD_KEY}O`, 'Open image'],
  [`${MOD_KEY},`, 'Preferences'],
  ['+ / −', 'Zoom in / out around the view centre'],
  ['1', 'Zoom to 100 %'],
  ['0', 'Fit the image to the window'],
  ['N', 'Show or hide the navigator'],
  ['Arrow keys', 'Pan by 50 px (Shift: by one view)'],
  ['Space + drag', 'Pan (also middle-button drag or the Pan tool)'],
  ['Mouse wheel', 'Zoom around the cursor'],
  ['Pinch', 'Zoom around the fingers'],
  ['Two-finger scroll', 'Pan'],
];

function ShortcutsContent() {
  return (
    <Table verticalSpacing={4}>
      <Table.Tbody>
        {SHORTCUTS.map(([keys, action]) => (
          <Table.Tr key={keys}>
            <Table.Td w={150}>
              <Kbd size="xs">{keys}</Kbd>
            </Table.Td>
            <Table.Td>
              <Text size="sm">{action}</Text>
            </Table.Td>
          </Table.Tr>
        ))}
      </Table.Tbody>
    </Table>
  );
}

function AboutContent() {
  const health = useQuery({
    queryKey: ['health'],
    queryFn: async ({ signal }) => (await (await fetch(`${API_PREFIX}/health`, { signal })).json()) as HealthResponse,
  });
  return (
    <Stack gap="xs">
      <Text size="sm">Haralick GLCM texture features for regions of interest in grayscale images.</Text>
      <InfoRows rows={[['Core version', health.data?.coreVersion ?? '…']]} />
      <Text size="xs" c="dimmed">
        Feature equations and references are in the Sphinx documentation (doc/).
      </Text>
    </Stack>
  );
}

function SamplesContent({ onClose }: { onClose(): void }) {
  const samples = useQuery({ queryKey: ['samples'], queryFn: ({ signal }) => getSamples(signal) });
  if (samples.isPending) {
    return <Text c="dimmed">Loading…</Text>;
  }
  if (samples.isError) {
    return <Text c="red">{samples.error.message}</Text>;
  }
  if (samples.data.samples.length === 0) {
    return <Text c="dimmed">The server has no sample images.</Text>;
  }

  const groups = new Map<string, SampleInfo[]>();
  for (const sample of samples.data.samples) {
    groups.set(sample.group, [...(groups.get(sample.group) ?? []), sample]);
  }
  return (
    <ScrollArea.Autosize mah="60vh">
      <Stack gap="xs">
        {[...groups].map(([group, entries]) => (
          <Stack key={group} gap={0}>
            <Text size="xs" c="dimmed" tt="uppercase" fw={600} px="xs">
              {group || 'General'}
            </Text>
            {entries.map((sample) => (
              <NavLink
                key={sample.path}
                label={sample.name}
                rightSection={
                  <Badge size="xs" variant="light" color="gray">
                    {formatBytes(sample.sizeBytes)}
                  </Badge>
                }
                onClick={() => {
                  onClose();
                  void openSample(sample.path);
                }}
              />
            ))}
          </Stack>
        ))}
      </Stack>
    </ScrollArea.Autosize>
  );
}

const TITLES: Record<ModalName, string> = {
  imageInfo: 'Image Info',
  preferences: 'Preferences',
  shortcuts: 'Keyboard Shortcuts',
  about: 'About GLCM Texture Analysis',
  samples: 'Open Sample Image',
};

export function AppModals() {
  const modal = useUi((state) => state.modal);
  const close = () => useUi.getState().setModal(null);
  return (
    <Modal opened={modal !== null} onClose={close} title={modal ? TITLES[modal] : ''} size={modal === 'imageInfo' ? 'lg' : 'md'}>
      {modal === 'imageInfo' && <ImageInfoContent />}
      {modal === 'preferences' && <PreferencesContent />}
      {modal === 'shortcuts' && <ShortcutsContent />}
      {modal === 'about' && <AboutContent />}
      {modal === 'samples' && <SamplesContent onClose={close} />}
    </Modal>
  );
}
