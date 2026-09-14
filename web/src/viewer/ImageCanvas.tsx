// Image canvas with zoom, pan and pixel readout (doc/ui-design-plan.md, section 6.1).

import type Konva from 'konva';
import { useCallback, useEffect, useRef, useState, type PointerEvent as ReactPointerEvent } from 'react';
import { Image as KonvaImage, Layer, Stage } from 'react-konva';
import { getPixel } from '../api/client';
import { sampleAt } from '../image/raw';
import { usePreferences } from '../stores/preferences';
import { useViewer } from '../stores/viewerStore';
import { keyToAction } from './keyboard';
import { Navigator } from './Navigator';
import { useDisplaySource } from './useDisplaySource';
import { pixelAt, zoomAt } from './viewport';
import { classifyWheel } from './wheel';

const PIXEL_LOOKUP_DELAY_MS = 80;

/** Safari's non-standard pinch events */
interface GestureEvent extends UIEvent {
  scale: number;
  clientX: number;
  clientY: number;
}

function isTextInput(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) {
    return false;
  }
  return target.isContentEditable || ['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName) || target.closest('[role="slider"]') !== null;
}

function isInOverlay(target: EventTarget | null): boolean {
  return target instanceof HTMLElement && target.closest('[role="dialog"], [role="menu"]') !== null;
}

export function ImageCanvas() {
  useDisplaySource();

  const containerRef = useRef<HTMLDivElement>(null);
  const imageNodeRef = useRef<Konva.Image>(null);
  const dragRef = useRef<{ pointerId: number; lastX: number; lastY: number } | null>(null);
  const pixelLookup = useRef<{ timer?: number; controller?: AbortController }>({});
  const [spaceHeld, setSpaceHeld] = useState(false);
  const [panning, setPanning] = useState(false);

  const image = useViewer((state) => state.image);
  const viewport = useViewer((state) => state.viewport);
  const viewSize = useViewer((state) => state.viewSize);
  const tool = useViewer((state) => state.tool);
  const displaySource = useViewer((state) => state.displaySource);
  const displayVersion = useViewer((state) => state.displayVersion);

  // Track the canvas size
  useEffect(() => {
    const element = containerRef.current!;
    const observer = new ResizeObserver(([entry]) => {
      useViewer.getState().setViewSize({ width: Math.floor(entry.contentRect.width), height: Math.floor(entry.contentRect.height) });
    });
    observer.observe(element);
    return () => observer.disconnect();
  }, []);

  // The renderer canvas is redrawn in place, which Konva cannot detect
  useEffect(() => {
    imageNodeRef.current?.getLayer()?.batchDraw();
  }, [displayVersion]);

  const updateHover = useCallback((clientX: number, clientY: number) => {
    const state = useViewer.getState();
    const element = containerRef.current;
    if (!state.image || !element) {
      return;
    }
    const rect = element.getBoundingClientRect();
    const pixel = pixelAt(state.viewport, { x: clientX - rect.left, y: clientY - rect.top }, state.image.info);
    const lookup = pixelLookup.current;
    if (!pixel) {
      window.clearTimeout(lookup.timer);
      lookup.controller?.abort();
      if (state.hover) {
        state.setHover(null);
      }
      return;
    }
    if (state.hover && state.hover.x === pixel.x && state.hover.y === pixel.y) {
      return;
    }
    if (state.image.raw) {
      state.setHover({ ...pixel, value: sampleAt(state.image.raw, pixel.x, pixel.y) });
      return;
    }

    // Large images: debounced server lookup
    state.setHover({ ...pixel, value: null });
    window.clearTimeout(lookup.timer);
    lookup.controller?.abort();
    const { imageId } = state.image.info;
    lookup.timer = window.setTimeout(async () => {
      const controller = new AbortController();
      lookup.controller = controller;
      try {
        const result = await getPixel(imageId, pixel.x, pixel.y, controller.signal);
        const hover = useViewer.getState().hover;
        if (hover && hover.x === result.x && hover.y === result.y && useViewer.getState().image?.info.imageId === imageId) {
          useViewer.getState().setHover({ x: result.x, y: result.y, value: result.value });
        }
      } catch {
        // Aborted or failed: the readout keeps showing "…"
      }
    }, PIXEL_LOOKUP_DELAY_MS);
  }, []);

  // Wheel, trackpad and Safari pinch; listeners are non-passive so the page does not scroll or zoom
  useEffect(() => {
    const element = containerRef.current!;
    const anchorOf = (clientX: number, clientY: number) => {
      const rect = element.getBoundingClientRect();
      return { x: clientX - rect.left, y: clientY - rect.top };
    };

    const onWheel = (event: WheelEvent) => {
      event.preventDefault();
      const state = useViewer.getState();
      if (!state.image) {
        return;
      }
      const action = classifyWheel(event, usePreferences.getState().scrollBehaviour);
      if (action.kind === 'zoom') {
        state.setViewport(zoomAt(state.viewport, action.factor, anchorOf(event.clientX, event.clientY)));
      } else {
        state.panBy(action.dx, action.dy);
      }
      updateHover(event.clientX, event.clientY);
    };

    let gestureScale = 1;
    const onGestureStart = (event: Event) => {
      event.preventDefault();
      gestureScale = 1;
    };
    const onGestureChange = (event: Event) => {
      event.preventDefault();
      const gesture = event as GestureEvent;
      const state = useViewer.getState();
      if (!state.image || gesture.scale <= 0) {
        return;
      }
      state.setViewport(zoomAt(state.viewport, gesture.scale / gestureScale, anchorOf(gesture.clientX, gesture.clientY)));
      gestureScale = gesture.scale;
    };

    element.addEventListener('wheel', onWheel, { passive: false });
    element.addEventListener('gesturestart', onGestureStart);
    element.addEventListener('gesturechange', onGestureChange);
    element.addEventListener('gestureend', onGestureStart);
    return () => {
      element.removeEventListener('wheel', onWheel);
      element.removeEventListener('gesturestart', onGestureStart);
      element.removeEventListener('gesturechange', onGestureChange);
      element.removeEventListener('gestureend', onGestureStart);
    };
  }, [updateHover]);

  // Keyboard shortcuts of the canvas; menus, dialogs and text fields keep their own keys
  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.defaultPrevented || isTextInput(event.target) || isInOverlay(event.target)) {
        return;
      }
      if (event.key === ' ') {
        if (event.target instanceof HTMLElement && event.target.closest('button, a, [role="menuitem"]')) {
          return;
        }
        event.preventDefault();
        setSpaceHeld(true);
        return;
      }
      const state = useViewer.getState();
      if (!state.image) {
        return;
      }
      const action = keyToAction(event, { hasSelection: false, view: state.viewSize });
      if (action) {
        event.preventDefault();
        state.runAction(action);
      }
    };
    const onKeyUp = (event: KeyboardEvent) => {
      if (event.key === ' ') {
        setSpaceHeld(false);
      }
    };
    const onBlur = () => setSpaceHeld(false);
    window.addEventListener('keydown', onKeyDown);
    window.addEventListener('keyup', onKeyUp);
    window.addEventListener('blur', onBlur);
    return () => {
      window.removeEventListener('keydown', onKeyDown);
      window.removeEventListener('keyup', onKeyUp);
      window.removeEventListener('blur', onBlur);
    };
  }, []);

  // Cancel a pending pixel lookup when the image changes
  useEffect(() => {
    const lookup = pixelLookup.current;
    return () => {
      window.clearTimeout(lookup.timer);
      lookup.controller?.abort();
    };
  }, [image]);

  const onPointerDown = (event: ReactPointerEvent<HTMLDivElement>) => {
    const panGesture = event.button === 1 || (event.button === 0 && (tool === 'pan' || spaceHeld));
    if (!image || !panGesture) {
      return;
    }
    event.preventDefault();
    event.currentTarget.setPointerCapture(event.pointerId);
    dragRef.current = { pointerId: event.pointerId, lastX: event.clientX, lastY: event.clientY };
    setPanning(true);
  };

  const onPointerMove = (event: ReactPointerEvent<HTMLDivElement>) => {
    const drag = dragRef.current;
    if (drag && drag.pointerId === event.pointerId) {
      useViewer.getState().panBy(event.clientX - drag.lastX, event.clientY - drag.lastY);
      drag.lastX = event.clientX;
      drag.lastY = event.clientY;
    }
    updateHover(event.clientX, event.clientY);
  };

  const endDrag = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (dragRef.current?.pointerId === event.pointerId) {
      dragRef.current = null;
      setPanning(false);
    }
  };

  const cursor = !image ? 'default' : panning ? 'grabbing' : tool === 'pan' || spaceHeld ? 'grab' : 'crosshair';
  const info = image?.info;

  return (
    <div
      ref={containerRef}
      className="canvas-area"
      data-cursor={cursor}
      data-testid="image-canvas"
      tabIndex={0}
      onPointerDown={onPointerDown}
      onPointerMove={onPointerMove}
      onPointerUp={endDrag}
      onPointerCancel={endDrag}
      onPointerLeave={() => {
        if (!dragRef.current && useViewer.getState().hover) {
          useViewer.getState().setHover(null);
        }
      }}
      onAuxClick={(event) => event.preventDefault()}
    >
      {viewSize.width > 0 && viewSize.height > 0 && (
        <Stage className="canvas-stage" width={viewSize.width} height={viewSize.height} listening={false}>
          {/* Sharp pixels when magnified; smoothing only when the image is shown smaller than its size */}
          <Layer imageSmoothingEnabled={viewport.scale < 1} listening={false}>
            {info && displaySource && (
              <KonvaImage
                ref={imageNodeRef}
                image={displaySource}
                x={viewport.x}
                y={viewport.y}
                width={info.width}
                height={info.height}
                scaleX={viewport.scale}
                scaleY={viewport.scale}
                listening={false}
                perfectDrawEnabled={false}
              />
            )}
          </Layer>
        </Stage>
      )}
      <Navigator />
    </div>
  );
}
