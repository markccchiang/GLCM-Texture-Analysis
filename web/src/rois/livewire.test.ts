import { describe, expect, it } from 'vitest';
import { joinSegments, pixelCentre } from './livewire';

describe('livewire', () => {
  it('uses pixel centres as anchors', () => {
    expect(pixelCentre({ x: 3, y: 7 })).toEqual([3.5, 7.5]);
  });

  it('joins segments that share their end points', () => {
    const segments: Array<Array<[number, number]>> = [
      [
        [1.5, 1.5],
        [5.5, 1.5],
      ],
      [
        [5.5, 1.5],
        [5.5, 6.5],
        [3.5, 8.5],
      ],
      [
        [3.5, 8.5],
        [1.5, 1.5],
      ],
    ];
    expect(joinSegments(segments, true)).toEqual([
      [1.5, 1.5],
      [5.5, 1.5],
      [5.5, 6.5],
      [3.5, 8.5],
    ]);
    expect(joinSegments(segments.slice(0, 2), false)).toEqual([
      [1.5, 1.5],
      [5.5, 1.5],
      [5.5, 6.5],
      [3.5, 8.5],
    ]);
    expect(joinSegments([], true)).toEqual([]);
    expect(joinSegments([[[2.5, 2.5]]], true)).toEqual([[2.5, 2.5]]);
  });
});
