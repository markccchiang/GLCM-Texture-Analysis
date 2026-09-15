// Results table (doc/ui-design-plan.md, section 6.3.3): sorting, column chooser, copy as TSV. Long tables render only the
// rows near the visible area, and each row follows the hovered ROI itself, so hovering does not re-render the table.
// The Plot view (ResultsPlot) charts the same measurements.

import { ActionIcon, Button, Checkbox, Group, Menu, SegmentedControl, Table, Text, Tooltip } from '@mantine/core';
import { notifications } from '@mantine/notifications';
import { IconClipboard, IconColumns, IconDownload, IconTrash } from '@tabler/icons-react';
import { useQuery } from '@tanstack/react-query';
import { useVirtualizer } from '@tanstack/react-virtual';
import { memo, useMemo, useRef, useState } from 'react';
import { CATALOG_QUERY } from '../api/queryClient';
import { NON_STANDARD_NOTE } from '../analysis/SettingsPanel';
import { PanelSection } from '../components/PanelSection';
import { exportResultsFile } from '../files/actions';
import { useRois } from '../rois/roiStore';
import { ResultsPlot } from './ResultsPlot';
import { useResults } from './resultsStore';
import { cellText, columnsForRows, rowsToTsv, sortRows, type ResultRow, type SortDirection } from './rows';

/** Tables with more rows render only the rows near the visible area */
export const VIRTUALIZE_ABOVE = 200;
const ESTIMATED_ROW_HEIGHT = 22;

type Column = ReturnType<typeof columnsForRows>[number];

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

function selectRoi(roiId: string): void {
  const rois = useRois.getState();
  if (rois.rois.some((roi) => roi.id === roiId)) {
    rois.select([roiId]);
  }
}

const ResultRowView = memo(function ResultRowView({
  row,
  columns,
  index,
  measure,
}: {
  row: ResultRow;
  columns: readonly Column[];
  index: number;
  measure?: (element: HTMLTableRowElement | null) => void;
}) {
  // A boolean per row: moving the pointer between ROIs re-renders the two affected rows, not the whole table
  const hovered = useRois((state) => state.hoveredId === row.roiId);
  return (
    <Table.Tr
      ref={measure}
      data-index={index}
      title={settingsSummary(row)}
      data-hovered={hovered || undefined}
      data-status={row.status}
      onMouseEnter={() => useRois.getState().setHovered(row.roiId)}
      onMouseLeave={() => useRois.getState().setHovered(null)}
      onClick={() => selectRoi(row.roiId)}
    >
      {columns.map((column) => (
        <Table.Td key={column.id} className={column.numeric ? 'mono' : undefined} style={{ textAlign: column.numeric ? 'right' : 'left', whiteSpace: 'nowrap' }}>
          {cellText(column, row)}
        </Table.Td>
      ))}
    </Table.Tr>
  );
});

export function ResultsPanel() {
  const rows = useResults((state) => state.rows);
  const hiddenColumns = useResults((state) => state.hiddenColumns);
  const catalog = useQuery(CATALOG_QUERY);
  const [sort, setSort] = useState<{ id: string; direction: SortDirection } | null>(null);
  const [view, setView] = useState<'table' | 'plot'>('table');
  const bodyRef = useRef<HTMLDivElement>(null);

  const columns = useMemo(() => columnsForRows(rows, catalog.data?.features ?? []), [rows, catalog.data]);
  const visible = useMemo(() => columns.filter((column) => !hiddenColumns.includes(column.id)), [columns, hiddenColumns]);
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

  const virtualize = sorted.length > VIRTUALIZE_ABOVE;
  const virtualizer = useVirtualizer({
    count: virtualize ? sorted.length : 0,
    getScrollElement: () => bodyRef.current,
    estimateSize: () => ESTIMATED_ROW_HEIGHT,
    overscan: 20,
  });
  const items = virtualize ? virtualizer.getVirtualItems() : [];
  const paddingTop = items.length > 0 ? items[0].start : 0;
  const paddingBottom = items.length > 0 ? virtualizer.getTotalSize() - items[items.length - 1].end : 0;
  const spacer = (height: number) => (
    <tr aria-hidden="true" className="results-spacer">
      <td colSpan={visible.length} style={{ height, padding: 0, border: 0 }} />
    </tr>
  );

  return (
    <PanelSection
      title={`Results${rows.length ? ` (${rows.length} rows)` : ''}`}
      bodyClassName="results-body"
      bodyRef={bodyRef}
      actions={
        <Group gap={4}>
          <SegmentedControl
            size="xs"
            aria-label="Results view"
            disabled={rows.length === 0}
            value={view}
            onChange={(value) => setView(value as 'table' | 'plot')}
            data={[
              { value: 'table', label: 'Table' },
              { value: 'plot', label: 'Plot' },
            ]}
          />
          {view === 'table' && (
            <>
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
            </>
          )}
          <Menu position="bottom-end">
            <Menu.Target>
              <Button size="compact-xs" variant="subtle" color="gray" leftSection={<IconDownload size={12} />} disabled={rows.length === 0}>
                Export
              </Button>
            </Menu.Target>
            <Menu.Dropdown>
              <Menu.Item onClick={() => void exportResultsFile('csv')}>CSV (settings in # lines)</Menu.Item>
              <Menu.Item onClick={() => void exportResultsFile('json')}>JSON (glcm-results)</Menu.Item>
            </Menu.Dropdown>
          </Menu>
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
      ) : view === 'plot' ? (
        <ResultsPlot />
      ) : (
        <Table stickyHeader striped={!virtualize} highlightOnHover withColumnBorders fz="xs" verticalSpacing={2} horizontalSpacing={6} data-testid="results-table">
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
            {virtualize ? (
              <>
                {paddingTop > 0 && spacer(paddingTop)}
                {items.map((item) => (
                  <ResultRowView key={sorted[item.index].key} row={sorted[item.index]} columns={visible} index={item.index} measure={virtualizer.measureElement} />
                ))}
                {paddingBottom > 0 && spacer(paddingBottom)}
              </>
            ) : (
              sorted.map((row, index) => <ResultRowView key={row.key} row={row} columns={visible} index={index} />)
            )}
          </Table.Tbody>
        </Table>
      )}
    </PanelSection>
  );
}
