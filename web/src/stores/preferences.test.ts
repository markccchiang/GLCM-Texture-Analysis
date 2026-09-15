import { beforeEach, describe, expect, it } from 'vitest';
import { usePreferences } from './preferences';

const store = () => usePreferences.getState();

beforeEach(() => {
  usePreferences.setState({ windowPresets: [] });
});

describe('window presets', () => {
  it('saves presets per bit depth and replaces one with the same name', () => {
    store().saveWindowPreset({ name: 'Lung', bitDepth: 16, min: 0, max: 1500 });
    store().saveWindowPreset({ name: 'Lung', bitDepth: 8, min: 10, max: 90 });
    store().saveWindowPreset({ name: 'Lung', bitDepth: 16, min: 100, max: 1600 });
    expect(store().windowPresets).toEqual([
      { name: 'Lung', bitDepth: 8, min: 10, max: 90 },
      { name: 'Lung', bitDepth: 16, min: 100, max: 1600 },
    ]);
  });

  it('removes only the preset of the given bit depth', () => {
    store().saveWindowPreset({ name: 'Soft tissue', bitDepth: 16, min: 900, max: 1300 });
    store().saveWindowPreset({ name: 'Soft tissue', bitDepth: 8, min: 20, max: 200 });
    store().removeWindowPreset('Soft tissue', 16);
    expect(store().windowPresets).toEqual([{ name: 'Soft tissue', bitDepth: 8, min: 20, max: 200 }]);
  });
});
