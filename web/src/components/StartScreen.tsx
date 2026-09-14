import { Anchor, Button, Center, Group, Stack, Text, Title } from '@mantine/core';
import { IconPhoto, IconUpload } from '@tabler/icons-react';
import { useQuery } from '@tanstack/react-query';
import { getSamples } from '../api/client';
import { openSample } from '../stores/imageLoader';
import { useUi } from '../stores/uiStore';

export function StartScreen() {
  const samples = useQuery({ queryKey: ['samples'], queryFn: ({ signal }) => getSamples(signal) });
  const defaultSample = samples.data?.defaultSample ?? null;

  return (
    <Center h="100%" p="md">
      <Stack align="center" gap="sm" maw={460}>
        <IconPhoto size={56} stroke={1.2} color="var(--mantine-color-dimmed)" />
        <Title order={3}>Open an image</Title>
        <Text c="dimmed" ta="center" size="sm">
          Drop a PNG, JPEG, BMP or 8/16-bit TIFF file anywhere in the window, or choose a file.
        </Text>
        <Group mt="xs">
          <Button leftSection={<IconUpload size={16} />} onClick={() => useUi.getState().requestOpenFile()}>
            Open Image…
          </Button>
          {defaultSample && (
            <Button variant="light" onClick={() => void openSample(defaultSample)}>
              Open sample image
            </Button>
          )}
        </Group>
        {(samples.data?.samples.length ?? 0) > 1 && (
          <Anchor component="button" size="sm" onClick={() => useUi.getState().setModal('samples')}>
            More sample images…
          </Anchor>
        )}
      </Stack>
    </Center>
  );
}
