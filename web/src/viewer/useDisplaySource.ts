// Keeps viewerStore.displaySource up to date with the current image and window (doc/ui-design-plan.md, 6.1):
// raw images are rendered in the browser, other images are fetched as display.png.

import { notifications } from '@mantine/notifications';
import { useEffect, useState } from 'react';
import { fetchDisplayBlob } from '../api/client';
import { BlobUrlCache } from '../image/blobUrlCache';
import { createRenderer, type ImageRenderer } from '../image/renderer';
import { usePreferences } from '../stores/preferences';
import { useViewer } from '../stores/viewerStore';

const DISPLAY_DEBOUNCE_MS = 150;
const displayUrls = new BlobUrlCache(20);

export function useDisplaySource(): void {
  const image = useViewer((state) => state.image);
  const windowRange = useViewer((state) => state.window);
  const useWebGl = usePreferences((state) => state.useWebGl);
  const [renderer, setRenderer] = useState<ImageRenderer | null>(null);
  const [rendererFailed, setRendererFailed] = useState(false);
  // The image whose WebGL context was lost (GPU reset, driver update, too many contexts). It is rendered with the
  // lookup table from then on; the next image tries WebGL again.
  const [contextLostImage, setContextLostImage] = useState<typeof image>(null);
  const webGlLost = image !== null && contextLostImage === image;

  // One renderer per raw image
  useEffect(() => {
    setRendererFailed(false);
    if (!image?.raw) {
      setRenderer(null);
      return;
    }
    let created: ImageRenderer;
    try {
      created = createRenderer(image.raw, {
        allowWebGl: useWebGl && !webGlLost,
        onContextLost: () => {
          console.warn('The WebGL context was lost; using the lookup-table renderer');
          setContextLostImage(image);
        },
      });
    } catch (error) {
      console.warn('No browser renderer available; using server rendering', error);
      setRenderer(null);
      setRendererFailed(true);
      return;
    }
    setRenderer(created);
    return () => {
      if (useViewer.getState().displaySource === created.canvas) {
        useViewer.getState().setDisplaySource(null, null);
      }
      created.dispose();
    };
  }, [image, useWebGl, webGlLost]);

  // Redraw at most once per frame when the window changes
  useEffect(() => {
    if (!renderer) {
      return;
    }
    const frame = requestAnimationFrame(() => {
      renderer.render(windowRange.min, windowRange.max);
      useViewer.getState().setDisplaySource(renderer.canvas, renderer.kind);
    });
    return () => cancelAnimationFrame(frame);
  }, [renderer, windowRange]);

  // display.png for images without raw samples
  const serverRendering = image !== null && (image.raw === null || rendererFailed);
  useEffect(() => {
    if (!image || !serverRendering) {
      return;
    }
    const { imageId } = image.info;
    const { min, max } = windowRange;
    const key = `${imageId}:${min}:${max}`;
    const controller = new AbortController();

    const show = (url: string) => {
      const element = new Image();
      element.onload = () => {
        if (!controller.signal.aborted && useViewer.getState().image === image) {
          useViewer.getState().setDisplaySource(element, 'server');
        }
      };
      element.src = url;
    };

    const cached = displayUrls.get(key);
    if (cached) {
      show(cached);
      return () => controller.abort();
    }

    // The first rendering is requested at once; window changes are debounced
    const delay = useViewer.getState().displaySource ? DISPLAY_DEBOUNCE_MS : 0;
    const timer = window.setTimeout(async () => {
      try {
        const blob = await fetchDisplayBlob(imageId, { min, max }, controller.signal);
        show(displayUrls.set(key, blob));
      } catch (error) {
        if (!controller.signal.aborted) {
          notifications.show({ color: 'red', title: 'Could not render the image', message: (error as Error).message });
        }
      }
    }, delay);
    return () => {
      window.clearTimeout(timer);
      controller.abort();
    };
  }, [image, serverRendering, windowRange]);
}
