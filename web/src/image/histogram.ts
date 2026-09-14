// Histogram drawing for the window/level control.

const round = (value: number) => Number(value.toFixed(2));

/**
 * SVG path of a step histogram filling width × height; heights are log(1 + count) by default, so sparse bins stay
 * visible next to a dominant background.
 */
export function histogramPath(histogram: readonly number[], width: number, height: number, scale: 'log' | 'linear' = 'log'): string {
  const values = histogram.map((count) => (scale === 'log' ? Math.log1p(count) : count));
  const peak = Math.max(0, ...values);
  if (histogram.length === 0 || peak === 0) {
    return `M0 ${height}H${width}`;
  }
  const binWidth = width / histogram.length;
  let path = `M0 ${height}`;
  values.forEach((value, i) => {
    path += `V${round(height - (value / peak) * height)}H${round((i + 1) * binWidth)}`;
  });
  return `${path}V${height}Z`;
}

/** Horizontal position of a sample value on a histogram of the given width */
export function valueToHistogramX(value: number, maxValue: number, width: number): number {
  return round((value / (maxValue + 1)) * width);
}
