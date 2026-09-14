// Overview map in the bottom-right corner of the canvas (doc/ui-design-plan.md, section 6.1).

import { useEffect, useRef, type PointerEvent as ReactPointerEvent } from 'react';
import { isNavigatorVisible, useViewer, type LoadedImage } from '../stores/viewerStore';
import { centreOn, visibleRect, type Point } from './viewport';

const THUMBNAIL_MAX = 200;

function NavigatorView({ image }: { image: LoadedImage }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const dragOffset = useRef<Point | null>(null);
  const viewport = useViewer((state) => state.viewport);
  const viewSize = useViewer((state) => state.viewSize);
  const displaySource = useViewer((state) => state.displaySource);
  const displayVersion = useViewer((state) => state.displayVersion);

  const { width, height } = image.info;
  const thumbScale = Math.min(THUMBNAIL_MAX / width, THUMBNAIL_MAX / height);
  const thumbWidth = Math.max(1, Math.round(width * thumbScale));
  const thumbHeight = Math.max(1, Math.round(height * thumbScale));

  // Downsampled copy of the current rendering (so it follows window/level)
  useEffect(() => {
    const context = canvasRef.current?.getContext('2d');
    if (!context || !displaySource) {
      return;
    }
    const frame = requestAnimationFrame(() => {
      context.imageSmoothingEnabled = true;
      context.imageSmoothingQuality = 'medium';
      context.clearRect(0, 0, thumbWidth, thumbHeight);
      context.drawImage(displaySource, 0, 0, thumbWidth, thumbHeight);
    });
    return () => cancelAnimationFrame(frame);
  }, [displaySource, displayVersion, thumbWidth, thumbHeight]);

  const visible = visibleRect(viewport, viewSize);
  const left = Math.max(0, visible.x * thumbScale);
  const top = Math.max(0, visible.y * thumbScale);
  const right = Math.min(thumbWidth, (visible.x + visible.width) * thumbScale);
  const bottom = Math.min(thumbHeight, (visible.y + visible.height) * thumbScale);

  const imagePointOf = (event: ReactPointerEvent<HTMLDivElement>): Point => {
    const rect = event.currentTarget.getBoundingClientRect();
    return { x: (event.clientX - rect.left) / thumbScale, y: (event.clientY - rect.top) / thumbScale };
  };

  const moveTo = (point: Point) => {
    const state = useViewer.getState();
    const offset = dragOffset.current ?? { x: 0, y: 0 };
    state.setViewport(centreOn(state.viewport, { x: point.x + offset.x, y: point.y + offset.y }, state.viewSize));
  };

  const onPointerDown = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (event.button !== 0) {
      return;
    }
    event.stopPropagation();
    event.currentTarget.setPointerCapture(event.pointerId);
    const point = imagePointOf(event);
    const insideView = point.x >= visible.x && point.x <= visible.x + visible.width && point.y >= visible.y && point.y <= visible.y + visible.height;
    // Dragging the rectangle keeps the grab point; clicking elsewhere centres the view there
    dragOffset.current = insideView ? { x: visible.x + visible.width / 2 - point.x, y: visible.y + visible.height / 2 - point.y } : { x: 0, y: 0 };
    moveTo(point);
  };

  const onPointerMove = (event: ReactPointerEvent<HTMLDivElement>) => {
    event.stopPropagation();
    if (dragOffset.current && event.currentTarget.hasPointerCapture(event.pointerId)) {
      moveTo(imagePointOf(event));
    }
  };

  const onPointerUp = (event: ReactPointerEvent<HTMLDivElement>) => {
    event.stopPropagation();
    dragOffset.current = null;
  };

  return (
    <div className="navigator" data-testid="navigator" onPointerMove={(event) => event.stopPropagation()}>
      <div
        className="navigator-thumb"
        style={{ width: thumbWidth, height: thumbHeight }}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={onPointerUp}
        onPointerCancel={onPointerUp}
      >
        <canvas ref={canvasRef} width={thumbWidth} height={thumbHeight} />
        {right > left && bottom > top && <div className="navigator-view" style={{ left, top, width: right - left, height: bottom - top }} />}
      </div>
    </div>
  );
}

export function Navigator() {
  const image = useViewer((state) => state.image);
  const visible = useViewer(isNavigatorVisible);
  return image && visible ? <NavigatorView image={image} /> : null;
}
