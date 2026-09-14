import { useHotkeys } from '@mantine/hooks';
import { useEffect, useRef, useState, type DragEvent } from 'react';
import { Group as PanelGroup, Panel, Separator, useDefaultLayout } from 'react-resizable-panels';
import { MenuBar } from './components/MenuBar';
import { AppModals } from './components/Modals';
import { AnalysisSettingsPlaceholder, ResultsPlaceholder, RoiManagerPlaceholder } from './components/Placeholders';
import { StatusBar } from './components/StatusBar';
import { Toolbar } from './components/Toolbar';
import { layoutStorage } from './layout/layoutStorage';
import { openImageFile } from './stores/imageLoader';
import { useUi } from './stores/uiStore';
import { CanvasArea } from './viewer/CanvasArea';

const ACCEPTED_TYPES = '.png,.jpg,.jpeg,.bmp,.tif,.tiff,image/png,image/jpeg,image/bmp,image/tiff';

function hasFiles(event: DragEvent): boolean {
  return Array.from(event.dataTransfer.types).includes('Files');
}

function Workspace() {
  const vertical = useDefaultLayout({ id: 'workspace-vertical', storage: layoutStorage });
  const horizontal = useDefaultLayout({ id: 'workspace-horizontal', storage: layoutStorage });

  return (
    <PanelGroup orientation="vertical" className="workspace" defaultLayout={vertical.defaultLayout} onLayoutChanged={vertical.onLayoutChanged}>
      <Panel id="main" minSize="30">
        <PanelGroup orientation="horizontal" defaultLayout={horizontal.defaultLayout} onLayoutChanged={horizontal.onLayoutChanged}>
          <Panel id="canvas" minSize="30">
            <CanvasArea />
          </Panel>
          <Separator className="separator separator-vertical" />
          <Panel id="sidebar" defaultSize={320} minSize={220} maxSize="50" collapsible>
            <PanelGroup orientation="vertical">
              <Panel id="roi-manager" minSize={80}>
                <RoiManagerPlaceholder />
              </Panel>
              <Separator className="separator separator-horizontal" />
              <Panel id="analysis-settings" minSize={80}>
                <AnalysisSettingsPlaceholder />
              </Panel>
            </PanelGroup>
          </Panel>
        </PanelGroup>
      </Panel>
      <Separator className="separator separator-horizontal" />
      <Panel id="results" defaultSize={150} minSize={60} collapsible>
        <ResultsPlaceholder />
      </Panel>
    </PanelGroup>
  );
}

export function App() {
  const fileInputRef = useRef<HTMLInputElement>(null);
  const [dragging, setDragging] = useState(false);
  const dragDepth = useRef(0);
  const layoutVersion = useUi((state) => state.layoutVersion);
  const openFileRequest = useUi((state) => state.openFileRequest);

  useEffect(() => {
    if (openFileRequest > 0) {
      fileInputRef.current?.click();
    }
  }, [openFileRequest]);

  useHotkeys([
    ['mod+O', () => useUi.getState().requestOpenFile()],
    ['mod+comma', () => useUi.getState().setModal('preferences')],
  ]);

  const onDragEnter = (event: DragEvent) => {
    if (hasFiles(event)) {
      dragDepth.current += 1;
      setDragging(true);
    }
  };
  const onDragLeave = (event: DragEvent) => {
    if (hasFiles(event)) {
      dragDepth.current = Math.max(0, dragDepth.current - 1);
      setDragging(dragDepth.current > 0);
    }
  };
  const onDragOver = (event: DragEvent) => {
    if (hasFiles(event)) {
      event.preventDefault();
      event.dataTransfer.dropEffect = 'copy';
    }
  };
  const onDrop = (event: DragEvent) => {
    if (!hasFiles(event)) {
      return;
    }
    event.preventDefault();
    dragDepth.current = 0;
    setDragging(false);
    const file = event.dataTransfer.files[0];
    if (file) {
      void openImageFile(file);
    }
  };

  return (
    <div className="app" onDragEnter={onDragEnter} onDragLeave={onDragLeave} onDragOver={onDragOver} onDrop={onDrop}>
      <MenuBar />
      <Toolbar />
      <main className="app-main">
        <Workspace key={layoutVersion} />
      </main>
      <StatusBar />
      <input
        ref={fileInputRef}
        type="file"
        accept={ACCEPTED_TYPES}
        hidden
        data-testid="open-image-input"
        onChange={(event) => {
          const file = event.currentTarget.files?.[0];
          event.currentTarget.value = '';
          if (file) {
            void openImageFile(file);
          }
        }}
      />
      <AppModals />
      {dragging && (
        <div className="drop-overlay">
          <div>Drop the image to open it</div>
        </div>
      )}
    </div>
  );
}
