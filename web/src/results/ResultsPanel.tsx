// Results table (doc/ui-design-plan.md, section 6.3.3): sorting, column chooser, copy as TSV.

import { ActionIcon, Button, Checkbox, Group, Menu, Table, Text, Tooltip } from '@mantine/core';
import { notifications } from '@mantine/notifications';
import { IconClipboard, IconColumns, IconTrash } from '@tabler/icons-react';
import { useQuery } from '@tanstack/react-query';
import { useMemo, useState } from 'react';
import { CATALOG_QUERY } from '../api/queryClient';
import { NON_STANDARD_NOTE } from '../analysis/SettingsPanel';
import { PanelSection } from '../components/PanelSection';
import { useRois } from '../rois/roiStore';
import { useResults } from './resultsStore';
import { cellText, columnsForRows, rowsToTsv, sortRows, type ResultRow, type SortDirection } from './rows';

const QUANTIZATION_LABELS: Record<string, string> = {
  fixedRange: 'fixed range',
  roiMinMax: 'ROI min–max',
  fixedBinWidth: 'fixed bin width',
  none: 'no quantization',
};

function settingsSummary(row: ResultRow): string {
  const { settings } = row;
  const quantization =
    settings.quantization.method === 'fixedRange'
      ? `fixed range ${settings.quantization.min}–${settings.quantization.max}`
      : settings.quantization.method === 'fixedBinWidth'
        ? `bin width ${settings.quantization.binWidth}`
        : QUANTIZATION_LABELS[settings.quantization.method];
  const lines = [
    `${row.imageName}`,
    `Ng ${settings.grayLevels}, ${quantization}, d ${row.distance}, directions ${settings.directions.join('/')}°, log ${settings.logBase}`,
  ];
  if (settings.score.enabled) {
    lines.push(`Score: age ${settings.score.age}, ${settings.score.profile} profile`);
  }
  if (row.error) {
    lines.push(`${row.status}: ${row.error}`);
  }
  lines.push(...row.warnings.map((warning) => `⚠ ${warning}`));
  return lines.join('\n');
}

export function ResultsPanel() {
  const rows = useResults((state) => state.rows);
  const hiddenColumns = useResults((state) => state.hiddenColumns);
  const hoveredRoiId = useRois((state) => state.hoveredId);
  const catalog = useQuery(CATALOG_QUERY);
  const [sort, setSort] = useState<{ id: string; direction: SortDirection } | null>(null);

  const columns = useMemo(() => columnsForRows(rows, catalog.data?.features ?? []), [rows, catalog.data]);
  const visible = columns.filter((column) => !hiddenColumns.includes(column.id));
  const sortColumn = sort ? columns.find((column) => column.id === sort.id) : undefined;
  const sorted = useMemo(() => (sort && sortColumn ? sortRows(rows, sortColumn, sort.direction) : rows), [rows, sort, sortColumn]);

  const toggleSort = (id: string) =>
    setSort((current) => (current?.id !== id ? { id, direction: 'asc' } : current.direction === 'asc' ? { id, direction: 'desc' } : null));

  const copy = async () => {
    try {
      await navigator.clipboard.writeText(rowsToTsv(sorted, visible));
      notifications.show({ color: 'green', message: `Copied ${sorted.length} rows as tab-separated text.` });
    } catch (error) {
      notifications.show({ color: 'red', title: 'Could not copy', message: (error as Error).message });
    }
  };

  const selectRoi = (roiId: string) => {
    const rois = useRois.getState();
    if (rois.rois.some((roi) => roi.id === roiId)) {
      rois.select([roiId]);
    }
  };

  return (
    <PanelSection
      title={`Results${rows.length ? ` (${rows.length} rows)` : ''}`}
      bodyClassName="results-body"
      actions={
        <Group gap={4}>
          <Menu position="bottom-end" closeOnItemClick={false}>
            <Menu.Target>
              <Button size="compact-xs" variant="subtle" color="gray" leftSection={<IconColumns size={12} />} disabled={columns.length === 0}>
                Columns
              </Button>
            </Menu.Target>
            <Menu.Dropdown mah={320} style={{ overflowY: 'auto' }}>
              {columns.map((column) => (
                <Menu.Item key={column.id} onClick={() => useResults.getState().toggleColumn(column.id)}>
                  <Checkbox size="xs" readOnly checked={!hiddenColumns.includes(column.id)} label={column.label} />
                </Menu.Item>
              ))}
            </Menu.Dropdown>
          </Menu>
          <Button size="compact-xs" variant="subtle" color="gray" leftSection={<IconClipboard size={12} />} disabled={rows.length === 0} onClick={copy}>
            Copy
          </Button>
          <Tooltip label="Export arrives with save/import/export (phase 4)">
            <Button size="compact-xs" variant="subtle" color="gray" disabled>
              Export
            </Button>
          </Tooltip>
          <ActionIcon size="sm" variant="subtle" color="gray" aria-label="Clear results" disabled={rows.length === 0} onClick={() => useResults.getState().clear()}>
            <IconTrash size={14} />
          </ActionIcon>
        </Group>
      }
    >
      {rows.length === 0 ? (
        <Text size="sm" c="dimmed" p="sm">
          Measurement results appear here. Select ROIs and press M, or ⇧M to measure all.
        </Text>
      ) : (
        <Table stickyHeader striped highlightOnHover withColumnBorders fz="xs" verticalSpacing={2} horizontalSpacing={6} data-testid="results-table">
          <Table.Thead>
            <Table.Tr>
              {visible.map((column) => (
                <Table.Th
                  key={column.id}
                  onClick={() => toggleSort(column.id)}
                  style={{ cursor: 'pointer', whiteSpace: 'nowrap', textAlign: column.numeric ? 'right' : 'left' }}
                  aria-sort={sort?.id === column.id ? (sort.direction === 'asc' ? 'ascending' : 'descending') : undefined}
                >
                  {column.label}
                  {column.nonStandard && (
                    <Tooltip label={NON_STANDARD_NOTE} multiline w={260}>
                      <span className="non-standard-mark"> ⚠</span>
                    </Tooltip>
                  )}
                  {sort?.id === column.id && (sort.direction === 'asc' ? ' ▲' : ' ▼')}
                </Table.Th>
              ))}
            </Table.Tr>
          </Table.Thead>
          <Table.Tbody>
            {sorted.map((row) => (
              <Table.Tr
                key={row.key}
                title={settingsSummary(row)}
                data-hovered={row.roiId === hoveredRoiId || undefined}
                data-status={row.status}
                onMouseEnter={() => useRois.getState().setHovered(row.roiId)}
                onMouseLeave={() => useRois.getState().setHovered(null)}
                onClick={() => selectRoi(row.roiId)}
              >
                {visible.map((column) => (
                  <Table.Td key={column.id} className={column.numeric ? 'mono' : undefined} style={{ textAlign: column.numeric ? 'right' : 'left', whiteSpace: 'nowrap' }}>
                    {cellText(column, row)}
                  </Table.Td>
                ))}
              </Table.Tr>
            ))}
          </Table.Tbody>
        </Table>
      )}
    </PanelSection>
  );
}
