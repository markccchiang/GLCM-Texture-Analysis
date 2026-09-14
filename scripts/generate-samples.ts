// Generates the synthetic test images in samples/synthetic/ (see samples/README.md).
//
//   npx tsx scripts/generate-samples.ts
//
// Every image is deterministic, so running the script again produces identical files.

import fs from 'node:fs/promises';
import path from 'node:path';
import { PNG } from 'pngjs';
import { encodeTiff } from '../bindings/node/test/tiff.js';

const OUTPUT = path.resolve(import.meta.dirname, '../samples/synthetic');

/** Deterministic pseudo-random numbers in [0, 1) (mulberry32) */
function random(seed: number): () => number {
  let state = seed >>> 0;
  return () => {
    state = (state + 0x6d2b79f5) >>> 0;
    let t = state;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

type Pixel = (x: number, y: number) => number;

function gray8(width: number, height: number, pixel: Pixel): number[] {
  const data: number[] = [];
  for (let y = 0; y < height; y += 1) {
    for (let x = 0; x < width; x += 1) {
      data.push(Math.max(0, Math.min(255, Math.round(pixel(x, y)))));
    }
  }
  return data;
}

function gray16(width: number, height: number, pixel: Pixel): number[] {
  const data: number[] = [];
  for (let y = 0; y < height; y += 1) {
    for (let x = 0; x < width; x += 1) {
      data.push(Math.max(0, Math.min(65535, Math.round(pixel(x, y)))));
    }
  }
  return data;
}

/** 8-bit grayscale PNG (color type 0) */
function pngGray(width: number, height: number, values: number[]): Buffer {
  const png = new PNG({ width, height, colorType: 0, inputColorType: 0, bitDepth: 8, inputHasAlpha: false });
  png.data = Buffer.from(values);
  return PNG.sync.write(png, { colorType: 0, inputColorType: 0, bitDepth: 8, inputHasAlpha: false });
}

/** 8-bit RGB PNG (color type 2) from interleaved RGB values */
function pngRgb(width: number, height: number, rgb: number[]): Buffer {
  const png = new PNG({ width, height, colorType: 2, inputColorType: 2, bitDepth: 8, inputHasAlpha: false });
  png.data = Buffer.from(rgb);
  return PNG.sync.write(png, { colorType: 2, inputColorType: 2, bitDepth: 8, inputHasAlpha: false });
}

interface Sample {
  file: string;
  data: () => Buffer;
}

const noise = random(20260914);
const noiseValues = gray8(256, 256, () => noise() * 256);
const noise16 = random(4095);

const samples: Sample[] = [
  {
    // Constant region: correlations, IMC and MCC use their fallback values
    file: 'constant-8bit.png',
    data: () => pngGray(128, 128, gray8(128, 128, () => 128)),
  },
  {
    // Black and white squares of 8 px. At distance 8, 0° and 90° pairs always differ (maximum Contrast) and diagonal
    // pairs never do (Contrast 0); at distance 16, Contrast is 0 in every direction.
    file: 'checkerboard-8px-8bit.png',
    data: () => pngGray(256, 256, gray8(256, 256, (x, y) => ((Math.floor(x / 8) + Math.floor(y / 8)) % 2 === 0 ? 0 : 255))),
  },
  {
    // Horizontal stripes 2 px high (period 4 px): Contrast is always 0 along 0°. The other directions are high at
    // distances 1 and 2 and drop to 0 at distance 4, the stripe period.
    file: 'stripes-horizontal-4px-8bit.png',
    data: () => pngGray(256, 256, gray8(256, 256, (_x, y) => (Math.floor(y / 2) % 2 === 0 ? 40 : 215))),
  },
  {
    // Diagonal stripes running along 45° (constant along x + y): Contrast is 0 along 45°
    file: 'stripes-diagonal-8bit.png',
    data: () => pngGray(256, 256, gray8(256, 256, (x, y) => (Math.floor((x + y) / 3) % 2 === 0 ? 30 : 225))),
  },
  {
    // Uniform noise over 0-255: high Entropy, Energy close to its minimum
    file: 'noise-uniform-8bit.png',
    data: () => pngGray(256, 256, noiseValues),
  },
  {
    // Left half: smooth horizontal gradient; right half: uniform noise. Draw an ROI on each side to compare textures.
    file: 'two-regions-8bit.png',
    data: () => pngGray(256, 256, gray8(256, 256, (x, y) => (x < 128 ? 60 + x : noiseValues[y * 256 + x]))),
  },
  {
    // Color image: tests the conversion to grayscale (red, green, blue, gray quadrants)
    file: 'quadrants-rgb.png',
    data: () => {
      const rgb: number[] = [];
      for (let y = 0; y < 128; y += 1) {
        for (let x = 0; x < 128; x += 1) {
          const quadrant = (y < 64 ? 0 : 2) + (x < 64 ? 0 : 1);
          rgb.push(...[[255, 0, 0], [0, 255, 0], [0, 0, 255], [128, 128, 128]][quadrant]);
        }
      }
      return pngRgb(128, 128, rgb);
    },
  },
  {
    // 16-bit ramp from left to right plus a slow vertical ripple, values 0-65000: tests window/level on 16-bit data
    file: 'gradient-16bit.tif',
    data: () =>
      encodeTiff({
        width: 512,
        height: 256,
        bitsPerSample: 16,
        samplesPerPixel: 1,
        data: gray16(512, 256, (x, y) => (x / 511) * 60000 + 2500 * (1 + Math.sin((2 * Math.PI * y) / 64))),
      }),
  },
  {
    // 12-bit values (0-4095) stored in a 16-bit TIFF, like many medical and scientific cameras: a smooth background with
    // a noisy disc in the centre. The default display window stretches the small range.
    file: 'disc-12bit-in-16bit.tif',
    data: () =>
      encodeTiff({
        width: 256,
        height: 256,
        bitsPerSample: 16,
        samplesPerPixel: 1,
        data: gray16(256, 256, (x, y) => {
          const inside = (x - 128) ** 2 + (y - 128) ** 2 < 70 ** 2;
          return inside ? 2000 + noise16() * 2000 : 800 + x * 2;
        }),
      }),
  },
  {
    // 4200 x 4200 pixels, above the server's raw-transfer limit (4096 x 4096): exercises display.png downscaling and
    // the /pixel endpoint instead of /raw
    file: 'large-4200x4200-8bit.png',
    data: () => pngGray(4200, 4200, gray8(4200, 4200, (x, y) => ((x >> 4) + (y >> 4)) % 256)),
  },
];

await fs.mkdir(OUTPUT, { recursive: true });
for (const sample of samples) {
  const data = sample.data();
  await fs.writeFile(path.join(OUTPUT, sample.file), data);
  console.log(`${sample.file.padEnd(34)} ${String(data.length).padStart(9)} bytes`);
}
