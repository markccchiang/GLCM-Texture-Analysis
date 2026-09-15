import { ActionIcon, Button, Divider, Menu, NumberInput, Tooltip } from '@mantine/core';
import {
  IconArrowsMaximize,
  IconBrush,
  IconChevronDown,
  IconEraser,
  IconHandStop,
  IconOvalVertical,
  IconPlayerPlay,
  IconPointer,
  IconPolygon,
  IconRuler,
  IconScribble,
  IconSquare,
  IconWand,
  IconZoomIn,
  IconZoomOut,
} from '@tabler/icons-react';
import type { ReactNode } from 'react';
import { measure } from '../analysis/measure';
import { useResults, isRunning } from '../results/resultsStore';
import { useViewer, type Tool } from '../stores/viewerStore';
import { WindowLevelControl } from './WindowLevelControl';

const ZOOM_PRESETS = [0.25, 0.5, 1, 2, 4, 8, 16, 32];

function ToolButton({ label, active, disabled, onClick, children }: { label: string; active?: boolean; disabled?: boolean; onClick?: () => void; children: ReactNode }) {
  return (
    <Tooltip label={label} openDelay={400}>
      <ActionIcon variant={active ? 'filled' : 'subtle'} color={active ? 'blue' : 'gray'} size="md" disabled={disabled} onClick={onClick} aria-label={label} aria-pressed={active}>
        {children}
      </ActionIcon>
    </Tooltip>
  );
}

const ROI_TOOLS: Array<{ tool: Tool; label: string; icon: ReactNode }> = [
  { tool: 'rectangle', label: 'Rectangle (R); Shift = square', icon: <IconSquare size={18} /> },
  { tool: 'ellipse', label: 'Ellipse (E); Shift = circle', icon: <IconOvalVertical size={18} /> },
  { tool: 'polygon', label: 'Polygon (P): click vertices, double-click or Enter to close', icon: <IconPolygon size={18} /> },
  { tool: 'freehand', label: 'Freehand (F)', icon: <IconScribble size={18} /> },
  { tool: 'wand', label: 'Magic wand (W): click a region; pixels connected to it within the tolerance', icon: <IconWand size={18} /> },
  { tool: 'brush', label: 'Brush (B): paint into the selected ROI, or a new ROI when none is selected', icon: <IconBrush size={18} /> },
  { tool: 'eraser', label: 'Eraser (X): remove a stroke from the selected ROI', icon: <IconEraser size={18} /> },
];

export function Toolbar() {
  const tool = useViewer((state) => state.tool);
  const wandTolerance = useViewer((state) => state.wandTolerance);
  const brushSize = useViewer((state) => state.brushSize);
  const scale = useViewer((state) => state.viewport.scale);
  const hasImage = useViewer((state) => state.image !== null);
  const running = useResults((state) => state.runs.some(isRunning));
  const viewer = useViewer.getState;
  const selectTool = (next: Tool) => () => viewer().setTool(next);

  return (
    <div className="toolbar" role="toolbar" aria-label="Tools">
      <ToolButton label="Pointer: select, move and edit ROIs" active={tool === 'pointer'} onClick={selectTool('pointer')}>
        <IconPointer size={18} />
      </ToolButton>
      <ToolButton label="Pan (hold Space)" active={tool === 'pan'} onClick={selectTool('pan')}>
        <IconHandStop size={18} />
      </ToolButton>

      <Divider orientation="vertical" my={8} />
      {ROI_TOOLS.map(({ tool: roiTool, label, icon }) => (
        <ToolButton key={roiTool} label={label} active={tool === roiTool} disabled={!hasImage} onClick={selectTool(roiTool)}>
          {icon}
        </ToolButton>
      ))}
      {tool === 'wand' && (
        <Tooltip label="Magic wand tolerance: largest difference from the clicked pixel value" openDelay={400}>
          <NumberInput
            size="xs"
            w={76}
            min={0}
            max={65535}
            allowDecimal={false}
            leftSection="±"
            aria-label="Wand tolerance"
            value={wandTolerance}
            onChange={(value) => typeof value === 'number' && viewer().setWandTolerance(value)}
          />
        </Tooltip>
      )}
      {(tool === 'brush' || tool === 'eraser') && (
        <Tooltip label="Brush and eraser diameter in image pixels" openDelay={400}>
          <NumberInput
            size="xs"
            w={76}
            min={1}
            max={2000}
            allowDecimal={false}
            leftSection="⌀"
            aria-label="Brush size"
            value={brushSize}
            onChange={(value) => typeof value === 'number' && viewer().setBrushSize(value)}
          />
        </Tooltip>
      )}
      <ToolButton label="Ruler (L): drag to measure a distance; Shift = 45° steps" active={tool === 'ruler'} disabled={!hasImage} onClick={selectTool('ruler')}>
        <IconRuler size={18} />
      </ToolButton>

      <Divider orientation="vertical" my={8} />
      <ToolButton label="Zoom out (−)" disabled={!hasImage} onClick={() => viewer().zoomStep(-1)}>
        <IconZoomOut size={18} />
      </ToolButton>
      <Menu position="bottom" width={110}>
        <Menu.Target>
          <Button variant="subtle" color="gray" size="compact-sm" w={78} disabled={!hasImage} rightSection={<IconChevronDown size={12} />} className="mono" aria-label="Zoom level">
            {hasImage ? `${Math.round(scale * 100)}%` : '–'}
          </Button>
        </Menu.Target>
        <Menu.Dropdown>
          {ZOOM_PRESETS.map((preset) => (
            <Menu.Item key={preset} onClick={() => viewer().zoomToScale(preset)}>
              {preset * 100} %
            </Menu.Item>
          ))}
        </Menu.Dropdown>
      </Menu>
      <ToolButton label="Zoom in (+)" disabled={!hasImage} onClick={() => viewer().zoomStep(1)}>
        <IconZoomIn size={18} />
      </ToolButton>
      <ToolButton label="Fit to window (0)" disabled={!hasImage} onClick={() => viewer().fit()}>
        <IconArrowsMaximize size={18} />
      </ToolButton>

      <Divider orientation="vertical" my={8} />
      <WindowLevelControl />

      <Button.Group ml="auto">
        <Button size="compact-sm" leftSection={<IconPlayerPlay size={14} />} disabled={!hasImage} loading={running} onClick={() => void measure('selected')} data-testid="measure-button">
          Measure
        </Button>
        <Menu position="bottom-end">
          <Menu.Target>
            <Button size="compact-sm" px={6} disabled={!hasImage} aria-label="Measure options">
              <IconChevronDown size={14} />
            </Button>
          </Menu.Target>
          <Menu.Dropdown>
            <Menu.Item onClick={() => void measure('selected')}>Measure selected (M)</Menu.Item>
            <Menu.Item onClick={() => void measure('all')}>Measure all (⇧M)</Menu.Item>
          </Menu.Dropdown>
        </Menu>
      </Button.Group>
    </div>
  );
}
