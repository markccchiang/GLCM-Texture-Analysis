// ROI ▸ ROI Classes…: the classes ROIs can belong to, with their names, colours and shortcuts

import { ActionIcon, Button, Group, Kbd, Stack, Text, TextInput } from '@mantine/core';
import { notifications } from '@mantine/notifications';
import { IconPlus, IconTrash } from '@tabler/icons-react';
import { ROI_COLORS } from './geometry';
import { MAX_CLASS_NAME_LENGTH, useRois } from './roiStore';

function commitName(current: string, next: string, input: HTMLInputElement): void {
  if (next.trim() === current) {
    input.value = current;
    return;
  }
  try {
    useRois.getState().updateClass(current, { name: next });
  } catch (error) {
    input.value = current;
    notifications.show({ color: 'yellow', title: 'Class not renamed', message: (error as Error).message });
  }
}

export function RoiClassesContent({ onClose }: { onClose(): void }) {
  const classes = useRois((state) => state.classes);
  const rois = useRois((state) => state.rois);
  const store = useRois.getState;

  return (
    <Stack gap="sm">
      <Text size="sm">
        Give ROIs a class, such as <em>lesion</em> or <em>normal</em>, to compare the groups. The class goes into the results table, the plots and
        every export. Select ROIs and press <Kbd size="xs">⇧</Kbd> with the class number to assign it, or <Kbd size="xs">⇧</Kbd>
        <Kbd size="xs">0</Kbd> to remove it; ROIs take the colour of their class.
      </Text>
      {classes.length === 0 ? (
        <Text size="sm" c="dimmed">
          No classes yet.
        </Text>
      ) : (
        <Stack gap={6} role="list" aria-label="Classes">
          {classes.map((roiClass, index) => {
            const count = rois.filter((roi) => roi.className === roiClass.name).length;
            return (
              <div key={roiClass.name} className="roi-class-row" role="listitem">
                <Text size="xs" c="dimmed" ta="center">
                  {index < 9 ? `⇧${index + 1}` : ''}
                </Text>
                <TextInput
                  size="xs"
                  aria-label={`Name of class ${index + 1}`}
                  defaultValue={roiClass.name}
                  maxLength={MAX_CLASS_NAME_LENGTH}
                  onBlur={(event) => commitName(roiClass.name, event.currentTarget.value, event.currentTarget)}
                  onKeyDown={(event) => {
                    if (event.key === 'Enter') {
                      event.currentTarget.blur();
                    }
                  }}
                />
                <Group gap={3} wrap="nowrap" role="group" aria-label={`Colour of ${roiClass.name}`}>
                  {ROI_COLORS.map((color) => (
                    <button
                      key={color}
                      type="button"
                      className="roi-colour-choice"
                      style={{ background: color }}
                      aria-label={`Colour ${color}`}
                      aria-pressed={roiClass.color.toUpperCase() === color}
                      onClick={() => store().updateClass(roiClass.name, { color })}
                    />
                  ))}
                </Group>
                <Group gap={4} wrap="nowrap">
                  <Text size="xs" c="dimmed" w={48} ta="right">
                    {count === 1 ? '1 ROI' : `${count} ROIs`}
                  </Text>
                  <ActionIcon size="sm" variant="subtle" color="gray" aria-label={`Delete class ${roiClass.name}`} onClick={() => store().removeClass(roiClass.name)}>
                    <IconTrash size={14} />
                  </ActionIcon>
                </Group>
              </div>
            );
          })}
        </Stack>
      )}
      <Group justify="space-between">
        <Button size="xs" variant="light" leftSection={<IconPlus size={12} />} onClick={() => store().addClass()}>
          Add class
        </Button>
        <Button size="xs" onClick={onClose}>
          Done
        </Button>
      </Group>
    </Stack>
  );
}
