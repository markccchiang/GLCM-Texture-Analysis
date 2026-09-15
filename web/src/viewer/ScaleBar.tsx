// Scale bar over the canvas when the open image has a pixel spacing; follows the zoom. With non-square pixels it gives
// the horizontal scale.

import { formatSpacing, isAnisotropic, scaleBar } from '../image/spacing';
import { usePreferences } from '../stores/preferences';
import { useViewer } from '../stores/viewerStore';

export function ScaleBar() {
  const hasImage = useViewer((state) => state.image !== null);
  const spacing = useViewer((state) => state.pixelSpacing);
  const scale = useViewer((state) => state.viewport.scale);
  const show = usePreferences((state) => state.showScaleBar);
  if (!hasImage || !spacing || !show) {
    return null;
  }
  const bar = scaleBar(spacing.x / scale);
  if (!bar) {
    return null;
  }
  const anisotropic = isAnisotropic(spacing);
  return (
    <div className="scale-bar" data-testid="scale-bar" title={`Pixels ${formatSpacing(spacing)}`}>
      <div className="scale-bar-line" style={{ width: bar.widthPx }} />
      <span className="mono">{anisotropic ? `${bar.label} horizontally` : bar.label}</span>
    </div>
  );
}
