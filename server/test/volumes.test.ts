import { createHash } from 'node:crypto';
import fs from 'node:fs/promises';
import path from 'node:path';
import type { AnalysisInfo, AnalysisSettings, ImageInfo, VolumeInfo } from '@glcm/api';
import { PNG } from 'pngjs';
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { encodeNifti, rampVolume } from '../../bindings/node/test/medical.js';
import { purgeExpired } from '../src/storage/retention.js';
import { createTestApp, multipartBody, type TestApp } from './helpers.js';

function uploadVolume(t: TestApp, fileName: string, data: Buffer) {
  const { payload, headers } = multipartBody(fileName, data, 'application/octet-stream');
  return t.app.inject({ method: 'POST', url: '/api/v1/volumes', payload, headers });
}

const RAMP = encodeNifti({ dimensions: [4, 3, 2, 2], dataType: 'int16', data: rampVolume(4, 3, 2, 2), voxelSize: [0.5, 0.75, 2], gzip: true });

describe('volumes', () => {
  let t: TestApp;

  beforeEach(async () => {
    t = await createTestApp();
  });

  afterEach(async () => {
    await t.close();
  });

  it('uploads a volume, previews slices and opens one as an image', async () => {
    const response = await uploadVolume(t, 'ramp.nii.gz', RAMP);
    expect(response.statusCode).toBe(201);
    const volume = response.json<VolumeInfo>();
    expect(volume.volumeId).toMatch(/^vol_[0-9a-f]{32}$/);
    expect(volume).toMatchObject({
      name: 'ramp.nii.gz',
      sizeBytes: RAMP.length,
      niftiVersion: 1,
      dimensions: [4, 3, 2],
      volumes: 2,
      dataType: 'int16',
      axisCodes: 'RAS',
      orientationSource: 'sform',
      acquisitionOrientation: 'axial',
      slices: {
        axial: { count: 2, width: 4, height: 3, pixelSpacing: { x: 0.5, y: 0.75 } },
        coronal: { count: 3, width: 4, height: 2, pixelSpacing: { x: 0.5, y: 2 } },
        sagittal: { count: 4, width: 3, height: 2, pixelSpacing: { x: 0.75, y: 2 } },
      },
      minimum: 0,
      maximum: 1123,
      bitDepth: 16,
      valueConversion: null,
      windowMin: 0,
      windowMax: 1123,
      warnings: [],
    });
    expect(volume).not.toHaveProperty('storage');
    expect((await t.app.inject({ method: 'GET', url: `/api/v1/volumes/${volume.volumeId}` })).json()).toEqual(volume);
    expect(await fs.readdir(path.join(t.dataDir, 'uploads'))).toEqual([]);

    const preview = await t.app.inject({ method: 'GET', url: `/api/v1/volumes/${volume.volumeId}/preview.png?orientation=sagittal&slice=3&volume=1` });
    expect(preview.statusCode).toBe(200);
    expect(preview.headers['content-type']).toBe('image/png');
    const decoded = PNG.sync.read(preview.rawPayload);
    expect([decoded.width, decoded.height]).toEqual([3, 2]);

    const outOfRange = await t.app.inject({ method: 'GET', url: `/api/v1/volumes/${volume.volumeId}/preview.png?orientation=axial&slice=2` });
    expect(outOfRange.statusCode).toBe(400);
    expect(outOfRange.json().message).toBe('The axial slice must be 0 to 1');

    const opened = await t.app.inject({
      method: 'POST',
      url: `/api/v1/volumes/${volume.volumeId}/images`,
      payload: { orientation: 'coronal', slice: 1, volume: 1 },
    });
    expect(opened.statusCode).toBe(201);
    const image = opened.json<ImageInfo>();
    expect(image).toMatchObject({
      name: 'ramp.nii.gz [coronal 1, volume 1]',
      width: 4,
      height: 2,
      bitDepth: 16,
      sourceChannels: 1,
      pixelSpacing: { x: 0.5, y: 2 },
      windowMin: 0,
      windowMax: 1123,
      transfer: 'raw',
    });
    expect(image).not.toHaveProperty('valueConversion');

    // Rows from superior: row 0 is k = 1
    const raw = await t.app.inject({ method: 'GET', url: `/api/v1/images/${image.imageId}/raw` });
    expect([...new Uint16Array(raw.rawPayload.buffer.slice(raw.rawPayload.byteOffset, raw.rawPayload.byteOffset + 16))]).toEqual([
      1110, 1111, 1112, 1113, 1010, 1011, 1012, 1013,
    ]);

    // The original is a PNG of the slice, which opens again as the same image with its spacing
    const original = await t.app.inject({ method: 'GET', url: `/api/v1/images/${image.imageId}/original` });
    expect(original.rawPayload.length).toBe(image.sizeBytes);
    expect(createHash('sha256').update(original.rawPayload).digest('hex')).toBe(image.sha256);
    const { payload, headers } = multipartBody('slice.png', original.rawPayload, 'image/png');
    const reopened = (await t.app.inject({ method: 'POST', url: '/api/v1/images', payload, headers })).json<ImageInfo>();
    expect(reopened).toMatchObject({ width: 4, height: 2, bitDepth: 16, pixelSpacing: { x: 0.5, y: 2 }, sha256: image.sha256 });

    const deleted = await t.app.inject({ method: 'DELETE', url: `/api/v1/volumes/${volume.volumeId}` });
    expect(deleted.statusCode).toBe(204);
    expect((await t.app.inject({ method: 'GET', url: `/api/v1/volumes/${volume.volumeId}` })).statusCode).toBe(404);
    expect(await fs.readdir(path.join(t.dataDir, 'volumes'))).toEqual([]);
    // The image stays
    expect((await t.app.inject({ method: 'GET', url: `/api/v1/images/${image.imageId}` })).statusCode).toBe(200);
  });

  it('names slices of 3D volumes without a volume number and writes the value conversion into exports', async () => {
    // uint8 0-3 with scl_slope 1 and scl_inter 0.5 would not be integral; slope 1 and intercept -1 give -1 ... 2
    const nifti = encodeNifti({ dimensions: [4, 4, 1], dataType: 'uint8', data: [0, 1, 2, 3, 3, 2, 1, 0, 0, 1, 2, 3, 3, 2, 1, 0], slope: 1, intercept: -1 });
    const volume = (await uploadVolume(t, 'tiny.nii', nifti)).json<VolumeInfo>();
    expect(volume.valueConversion).toEqual({
      scale: 1,
      offset: -1024,
      unit: '',
      description: 'Rescaled with scl_slope 1, scl_inter -1; values stored + 1024; value = stored value - 1024',
    });
    const image = (
      await t.app.inject({ method: 'POST', url: `/api/v1/volumes/${volume.volumeId}/images`, payload: { orientation: 'axial', slice: 0 } })
    ).json<ImageInfo>();
    expect(image.name).toBe('tiny.nii [axial 0]');
    expect(image.valueConversion).toEqual(volume.valueConversion);

    const settings: AnalysisSettings = {
      features: ['Mean', 'Contrast'],
      grayLevels: 4,
      quantization: { method: 'roiMinMax', min: 0, max: 65535, binWidth: 0 },
      distances: [1],
      directions: [0, 45, 90, 135],
      aggregation: 'meanOnly',
      logBase: 'natural',
      score: { enabled: false, age: 40, coefficients: [1.138, -1.814, 1.416, 1.714], profile: 'calibration', intensityMin: 0, intensityMax: 255 },
    };
    const started = await t.app.inject({
      method: 'POST',
      url: '/api/v1/analyses',
      payload: { imageId: image.imageId, rois: [{ id: 'all', name: 'All', shape: { type: 'rectangle', x: 0, y: 0, width: 4, height: 4 } }], settings },
    });
    expect(started.statusCode).toBe(202);
    const analysis = started.json<AnalysisInfo>();
    expect(analysis.valueConversion).toBe(volume.valueConversion!.description);
    await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${analysis.analysisId}/events` });
    const csv = await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${analysis.analysisId}/results.csv` });
    expect(csv.body).toContain('# valueConversion=Rescaled with scl_slope 1, scl_inter -1; values stored + 1024; value = stored value - 1024\n');
    const json = await t.app.inject({ method: 'GET', url: `/api/v1/analyses/${analysis.analysisId}/results.json` });
    expect(json.json().image.valueConversion).toBe(volume.valueConversion!.description);
  });

  it('refuses files that are not NIfTI volumes, or too large', async () => {
    const png = await uploadVolume(t, 'image.png', Buffer.from('not a volume at all, just some bytes that are long enough'));
    expect(png.statusCode).toBe(422);
    expect(png.json()).toEqual({ error: 'InvalidImage', message: 'The file could not be read as a NIfTI volume (.nii or .nii.gz)' });
    expect(await fs.readdir(path.join(t.dataDir, 'volumes'))).toEqual([]);

    const limited = await createTestApp({ maxVolumeBytes: 95 });
    try {
      const response = await uploadVolume(limited, 'ramp.nii.gz', RAMP);
      expect(response.statusCode).toBe(422);
      expect(response.json()).toEqual({ error: 'ImageTooLarge', message: 'The volume has 96 bytes of voxel data, more than the limit of 95 bytes' });
    } finally {
      await limited.close();
    }

    const small = await createTestApp({ maxImagePixels: 11 });
    try {
      const volume = (await uploadVolume(small, 'ramp.nii.gz', RAMP)).json<VolumeInfo>();
      const tooLarge = await small.app.inject({ method: 'POST', url: `/api/v1/volumes/${volume.volumeId}/images`, payload: { orientation: 'axial', slice: 0 } });
      expect(tooLarge.statusCode).toBe(422);
      expect(tooLarge.json().error).toBe('ImageTooLarge');
    } finally {
      await small.close();
    }

    expect((await t.app.inject({ method: 'GET', url: '/api/v1/volumes/vol_00000000000000000000000000000000' })).statusCode).toBe(404);
    expect((await t.app.inject({ method: 'GET', url: '/api/v1/volumes/img_00000000000000000000000000000000' })).statusCode).toBe(400);
  });

  it('removes volumes left behind by retention and on restart', async () => {
    const volume = (await uploadVolume(t, 'ramp.nii.gz', RAMP)).json<VolumeInfo>();
    const { VolumeStore } = await import('../src/storage/VolumeStore.js');
    const volumes = new VolumeStore(t.dataDir);
    const images = { list: async () => [], remove: async () => false };
    const results = { removeFinishedBefore: async () => 0 };
    const jobs = { forgetFinishedBefore: () => undefined };
    await purgeExpired({ images, volumes, results, jobs } as never, 60_000, Date.parse(volume.createdAt) + 30_000);
    expect(await volumes.get(volume.volumeId)).toBeDefined();
    await purgeExpired({ images, volumes, results, jobs } as never, 60_000, Date.parse(volume.createdAt) + 90_000);
    expect(await volumes.get(volume.volumeId)).toBeUndefined();

    await uploadVolume(t, 'ramp.nii.gz', RAMP);
    await volumes.init();
    expect(await volumes.list()).toEqual([]);
  });
});
