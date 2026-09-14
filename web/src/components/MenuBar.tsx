import { Badge, Button, Menu, Text } from '@mantine/core';
import type { ReactNode } from 'react';
import { clearStoredLayouts } from '../layout/layoutStorage';
import { cancelImageLoad } from '../stores/imageLoader';
import { MOD_KEY, useUi } from '../stores/uiStore';
import { isNavigatorVisible, useViewer } from '../stores/viewerStore';

function Shortcut({ children }: { children: ReactNode }) {
  return (
    <Text size="xs" c="dimmed">
      {children}
    </Text>
  );
}

function TopMenu({ label, children }: { label: string; children: ReactNode }) {
  return (
    <Menu position="bottom-start" offset={2} shadow="md" width={250} trigger="click-hover" openDelay={0} closeDelay={150}>
      <Menu.Target>
        <Button variant="subtle" color="gray" size="compact-sm">
          {label}
        </Button>
      </Menu.Target>
      <Menu.Dropdown>{children}</Menu.Dropdown>
    </Menu>
  );
}

const LATER = { phase3: 'Arrives with ROI tools (phase 3)', phase4: 'Arrives with analysis (phase 4)', phase6: 'Arrives with projects (phase 6)' };

function Later({ children, phase }: { children: ReactNode; phase: keyof typeof LATER }) {
  return (
    <Menu.Item disabled title={LATER[phase]}>
      {children}
    </Menu.Item>
  );
}

export function MenuBar() {
  const hasImage = useViewer((state) => state.image !== null);
  const navigatorVisible = useViewer(isNavigatorVisible);
  const viewer = useViewer.getState;
  const ui = useUi.getState;
  const local = ['localhost', '127.0.0.1', '[::1]'].includes(window.location.hostname);

  return (
    <nav className="menu-bar" aria-label="Main menu">
      <Text fw={700} size="sm" mr="sm">
        ▣ GLCM Texture Analysis
      </Text>

      <TopMenu label="File">
        <Menu.Item rightSection={<Shortcut>{MOD_KEY}O</Shortcut>} onClick={() => ui().requestOpenFile()}>
          Open Image…
        </Menu.Item>
        <Menu.Item onClick={() => ui().setModal('samples')}>Open Sample Image…</Menu.Item>
        <Later phase="phase6">Open Project…</Later>
        <Later phase="phase6">Save Project</Later>
        <Menu.Divider />
        <Later phase="phase4">Export Results</Later>
        <Menu.Divider />
        <Menu.Item
          disabled={!hasImage}
          onClick={() => {
            cancelImageLoad();
            viewer().closeImage();
          }}
        >
          Close Image
        </Menu.Item>
      </TopMenu>

      <TopMenu label="Edit">
        <Later phase="phase3">Undo</Later>
        <Later phase="phase3">Redo</Later>
        <Later phase="phase3">Select All ROIs</Later>
        <Menu.Divider />
        <Menu.Item rightSection={<Shortcut>{MOD_KEY},</Shortcut>} onClick={() => ui().setModal('preferences')}>
          Preferences…
        </Menu.Item>
      </TopMenu>

      <TopMenu label="Image">
        <Menu.Item disabled={!hasImage} rightSection={<Shortcut>+</Shortcut>} onClick={() => viewer().zoomStep(1)}>
          Zoom In
        </Menu.Item>
        <Menu.Item disabled={!hasImage} rightSection={<Shortcut>−</Shortcut>} onClick={() => viewer().zoomStep(-1)}>
          Zoom Out
        </Menu.Item>
        <Menu.Item disabled={!hasImage} rightSection={<Shortcut>1</Shortcut>} onClick={() => viewer().zoomToScale(1)}>
          Zoom 100 %
        </Menu.Item>
        <Menu.Item disabled={!hasImage} rightSection={<Shortcut>0</Shortcut>} onClick={() => viewer().fit()}>
          Fit to Window
        </Menu.Item>
        <Later phase="phase3">Zoom to Selection</Later>
        <Menu.Divider />
        <Menu.Label>Window/Level</Menu.Label>
        <Menu.Item disabled={!hasImage} onClick={() => viewer().resetWindow('auto')}>
          Auto (0.5–99.5 %)
        </Menu.Item>
        <Menu.Item disabled={!hasImage} onClick={() => viewer().resetWindow('full')}>
          Full Range
        </Menu.Item>
        <Menu.Item disabled={!hasImage} onClick={() => ui().setWindowPanelOpen(true)}>
          Custom…
        </Menu.Item>
        <Menu.Divider />
        <Menu.Item disabled={!hasImage} onClick={() => ui().setModal('imageInfo')}>
          Image Info
        </Menu.Item>
      </TopMenu>

      <TopMenu label="ROI">
        <Later phase="phase3">Rectangle</Later>
        <Later phase="phase3">Ellipse</Later>
        <Later phase="phase3">Polygon</Later>
        <Later phase="phase3">Freehand</Later>
        <Menu.Divider />
        <Later phase="phase3">Import ROI Set…</Later>
        <Later phase="phase3">Export ROI Set…</Later>
      </TopMenu>

      <TopMenu label="Analyze">
        <Later phase="phase4">Measure Selected</Later>
        <Later phase="phase4">Measure All</Later>
        <Later phase="phase4">Clear Results</Later>
      </TopMenu>

      <TopMenu label="View">
        <Menu.Item disabled={!hasImage} rightSection={<Shortcut>N</Shortcut>} onClick={() => viewer().toggleNavigator()}>
          {navigatorVisible ? 'Hide Navigator' : 'Show Navigator'}
        </Menu.Item>
        <Menu.Divider />
        <Menu.Item
          onClick={() => {
            clearStoredLayouts();
            ui().resetLayout();
          }}
        >
          Reset Layout
        </Menu.Item>
      </TopMenu>

      <TopMenu label="Help">
        <Menu.Item onClick={() => ui().setModal('shortcuts')}>Keyboard Shortcuts</Menu.Item>
        <Menu.Item onClick={() => ui().setModal('about')}>About</Menu.Item>
      </TopMenu>

      <Badge ml="auto" variant="dot" color={local ? 'green' : 'blue'} size="sm">
        {local ? 'Local' : window.location.hostname}
      </Badge>
    </nav>
  );
}
