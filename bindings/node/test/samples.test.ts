import fs from 'node:fs/promises';
import path from 'node:path';
import { describe, expect, it } from 'vitest';
import * as native from '../index.js';

const SAMPLES = path.resolve(import.meta.dirname, '../../../samples');

// Every image in samples/ (see samples/README.md): path, width, height, bit depth, channels before grayscale conversion
const EXPECTED: ReadonlyArray<readonly [file: string, width: number, height: number, bitDepth: 8 | 16, channels: number]> = [
  ['lena.jpg', 650, 366, 8, 3],
  ['synthetic/checkerboard-8px-8bit.png', 256, 256, 8, 1],
  ['synthetic/constant-8bit.png', 128, 128, 8, 1],
  ['synthetic/disc-12bit-in-16bit.tif', 256, 256, 16, 1],
  ['synthetic/gradient-16bit.tif', 512, 256, 16, 1],
  ['synthetic/large-4200x4200-8bit.png', 4200, 4200, 8, 1],
  ['synthetic/noise-uniform-8bit.png', 256, 256, 8, 1],
  ['synthetic/quadrants-rgb.png', 128, 128, 8, 3],
  ['synthetic/stripes-diagonal-8bit.png', 256, 256, 8, 1],
  ['synthetic/stripes-horizontal-4px-8bit.png', 256, 256, 8, 1],
  ['synthetic/two-regions-8bit.png', 256, 256, 8, 1],
  ['textures/brick.png', 512, 512, 8, 1],
  ['textures/camera.png', 512, 512, 8, 1],
  ['textures/coins.png', 384, 303, 8, 1],
  ['textures/grass.png', 512, 512, 8, 1],
  ['textures/gravel.png', 512, 512, 8, 1],
];

const IMAGE_EXTENSIONS = new Set(['.png', '.jpg', '.jpeg', '.tif', '.tiff', '.bmp']);

describe('sample images', () => {
  it('lists every image in samples/', async () => {
    const entries = await fs.readdir(SAMPLES, { recursive: true });
    const images = entries.filter((entry) => IMAGE_EXTENSIONS.has(path.extname(entry).toLowerCase())).map((entry) => entry.split(path.sep).join('/'));
    expect(images.sort()).toEqual(EXPECTED.map(([file]) => file).sort());
  });

  it.each(EXPECTED)('decodes %s', async (file, width, height, bitDepth, channels) => {
    const image = await native.decodeImageFile(path.join(SAMPLES, file));
    expect(image).toMatchObject({ width, height, bitDepth, sourceChannels: channels });
    expect(image.pixels).toHaveLength(width * height * (bitDepth / 8));
  });

  it('converts the RGB quadrants to BT.601 gray values', async () => {
    const image = await native.decodeImageFile(path.join(SAMPLES, 'synthetic/quadrants-rgb.png'));
    const at = (x: number, y: number) => image.pixels[y * image.width + x];
    expect([at(10, 10), at(100, 10), at(10, 100), at(100, 100)]).toEqual([76, 150, 29, 128]);
  });
});
