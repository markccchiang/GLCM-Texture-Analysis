/**
 * Window/level mapping of one intensity to an 8-bit display value, identical to glcm::WindowLevel in
 * core/imaging/DisplayRenderer (doc/ui-design-plan.md, section 6.1). windowMin and windowMax are integers with
 * windowMin <= windowMax.
 *
 * - windowMax === windowMin: 255 if value >= windowMin, otherwise 0
 * - otherwise: clamp(floor(((value - min) * 510 + (max - min)) / (2 * (max - min))), 0, 255)
 *
 * All intermediate values stay far below 2^53, so the result is exact.
 */
export function windowLevel(value: number, windowMin: number, windowMax: number): number {
  if (windowMax === windowMin) {
    return value >= windowMin ? 255 : 0;
  }
  if (value <= windowMin) {
    return 0;
  }
  if (value >= windowMax) {
    return 255;
  }
  const width = windowMax - windowMin;
  return Math.floor(((value - windowMin) * 510 + width) / (2 * width));
}
