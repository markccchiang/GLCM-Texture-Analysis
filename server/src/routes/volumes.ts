import fs from 'node:fs/promises';
import { createHash } from 'node:crypto';
import type { FastifyPluginAsyncTypebox } from '@fastify/type-provider-typebox';
import {
  ErrorResponse,
  ImageInfo,
  sliceImageName,
  VolumeIdParams,
  VolumeInfo,
  VolumePreviewQuery,
  VolumeSliceRequest,
  type SliceOrientation,
} from '@glcm/api';
import * as native from '@glcm/native';
import { Type } from 'typebox';
import type { ServerConfig } from '../config.js';
import { ApiError } from '../errors.js';
import { newImageId, type ImageStore } from '../storage/ImageStore.js';
import { newVolumeId, type StoredVolume, type VolumeStore } from '../storage/VolumeStore.js';
import { withUpload } from '../uploads.js';

export interface VolumeRoutesOptions {
  config: ServerConfig;
  store: ImageStore;
  volumes: VolumeStore;
}

export const volumeRoutes: FastifyPluginAsyncTypebox<VolumeRoutesOptions> = async (app, { config, store, volumes }) => {
  async function requireVolume(id: string): Promise<StoredVolume> {
    const volume = await volumes.get(id);
    if (!volume) {
      throw new ApiError(404, 'NotFound', `Volume ${id} was not found`);
    }
    return volume;
  }

  function extractSlice(volume: StoredVolume, orientation: SliceOrientation, slice: number, volumeIndex: number, encodePng: boolean) {
    const geometry = volume.info.slices[orientation];
    if (slice >= geometry.count) {
      throw new ApiError(400, 'BadRequest', `The ${orientation} slice must be 0 to ${geometry.count - 1}`);
    }
    if (volumeIndex >= volume.info.volumes) {
      throw new ApiError(400, 'BadRequest', `The volume must be 0 to ${volume.info.volumes - 1}`);
    }
    if (geometry.width * geometry.height > config.maxImagePixels) {
      throw new ApiError(
        422,
        'ImageTooLarge',
        `The ${orientation} slices have ${geometry.width * geometry.height} pixels, more than the limit of ${config.maxImagePixels}`,
      );
    }
    return native.extractNiftiSlice(volumes.volumePath(volume.info.volumeId), {
      orientation,
      slice,
      volume: volumeIndex,
      storage: volume.storage,
      maxPixels: config.maxImagePixels,
      encodePng,
    });
  }

  app.post(
    '/volumes',
    {
      schema: {
        summary: 'Upload a NIfTI volume',
        description:
          'multipart/form-data with one .nii or .nii.gz file (NIfTI-1 or NIfTI-2). The volume is kept until it is deleted, so slices can be previewed and opened as images.',
        tags: ['volumes'],
        consumes: ['multipart/form-data'],
        response: { 201: VolumeInfo, 400: ErrorResponse, 413: ErrorResponse, 415: ErrorResponse, 422: ErrorResponse },
      },
    },
    async (request, reply) =>
      withUpload(request, store, config, 'volume', async (upload) => {
        const volumeId = newVolumeId();
        const copyPath = await volumes.create(volumeId);
        try {
          let inspected: native.NativeVolumeInfo;
          try {
            inspected = await native.inspectNiftiVolume(upload.path, copyPath, { maxBytes: config.maxVolumeBytes });
          } catch (error) {
            const code = (error as { code?: string }).code;
            if (code === 'IMAGE_TOO_LARGE') {
              throw new ApiError(422, 'ImageTooLarge', (error as Error).message);
            }
            if (code === 'UNSUPPORTED_IMAGE') {
              throw new ApiError(422, 'UnsupportedImage', (error as Error).message);
            }
            throw new ApiError(422, 'InvalidImage', 'The file could not be read as a NIfTI volume (.nii or .nii.gz)');
          }
          const info: VolumeInfo = {
            volumeId,
            name: upload.name,
            sizeBytes: upload.sizeBytes,
            niftiVersion: inspected.version,
            dimensions: inspected.dimensions,
            volumes: inspected.volumes,
            dataType: inspected.dataType,
            axisCodes: inspected.axisCodes,
            orientationSource: inspected.orientationSource,
            acquisitionOrientation: inspected.acquisitionOrientation,
            slices: inspected.slices,
            minimum: inspected.minimum,
            maximum: inspected.maximum,
            bitDepth: inspected.storage.bitDepth,
            valueConversion: inspected.valueConversion,
            windowMin: inspected.windowMin,
            windowMax: inspected.windowMax,
            warnings: inspected.warnings,
            createdAt: new Date().toISOString(),
          };
          await volumes.save({ info, storage: inspected.storage });
          return reply.code(201).send(info);
        } catch (error) {
          await volumes.remove(volumeId);
          throw error;
        }
      }),
  );

  app.get(
    '/volumes/:id',
    {
      schema: {
        summary: 'Volume metadata',
        tags: ['volumes'],
        params: VolumeIdParams,
        response: { 200: VolumeInfo, 400: ErrorResponse, 404: ErrorResponse },
      },
    },
    async (request) => (await requireVolume(request.params.id)).info,
  );

  app.delete(
    '/volumes/:id',
    {
      schema: {
        summary: 'Delete a volume',
        description: 'Images opened from the volume are kept.',
        tags: ['volumes'],
        params: VolumeIdParams,
        response: { 204: Type.Unsafe<undefined>({ type: 'null', description: 'Deleted (no body)' }), 400: ErrorResponse, 404: ErrorResponse },
      },
    },
    async (request, reply) => {
      if (!(await volumes.remove(request.params.id))) {
        throw new ApiError(404, 'NotFound', `Volume ${request.params.id} was not found`);
      }
      return reply.code(204).send(undefined);
    },
  );

  app.get(
    '/volumes/:id/preview.png',
    {
      schema: {
        summary: '8-bit rendering of one slice',
        description: "Rendered with the volume's default window and downscaled so the long side is at most maxSize.",
        tags: ['volumes'],
        params: VolumeIdParams,
        querystring: VolumePreviewQuery,
        response: {
          200: { description: 'PNG image', content: { 'image/png': { schema: Type.Unsafe<Buffer>({ type: 'string', format: 'binary' }) } } },
          400: ErrorResponse,
          404: ErrorResponse,
          422: ErrorResponse,
        },
      },
    },
    async (request, reply) => {
      const volume = await requireVolume(request.params.id);
      const { orientation, slice, volume: volumeIndex = 0 } = request.query;
      const image = await extractSlice(volume, orientation, slice, volumeIndex, false);
      const maxSize = Math.min(request.query.maxSize ?? config.displayMaxSize, config.displayMaxSize);
      const png = await native.renderDisplay(image.pixels, image.width, image.height, image.bitDepth, volume.info.windowMin, volume.info.windowMax, maxSize);
      // A volume never changes
      return reply.header('Cache-Control', 'private, max-age=3600').type('image/png').send(png);
    },
  );

  app.post(
    '/volumes/:id/images',
    {
      schema: {
        summary: 'Open a slice as an image',
        description:
          'Stores one slice as a 2D image, as if it had been uploaded: its original file is a 16-bit (or 8-bit) PNG of the slice with the pixel spacing.',
        tags: ['volumes'],
        params: VolumeIdParams,
        body: VolumeSliceRequest,
        response: { 201: ImageInfo, 400: ErrorResponse, 404: ErrorResponse, 422: ErrorResponse },
      },
    },
    async (request, reply) => {
      const volume = await requireVolume(request.params.id);
      const { orientation, slice, volume: volumeIndex = 0 } = request.body;
      const image = await extractSlice(volume, orientation, slice, volumeIndex, true);
      const png = image.png!;
      const pixelCount = image.width * image.height;
      const info: ImageInfo = {
        imageId: newImageId(),
        name: sliceImageName(volume.info.name, orientation, slice, volumeIndex, volume.info.volumes),
        sizeBytes: png.length,
        width: image.width,
        height: image.height,
        bitDepth: image.bitDepth,
        sourceChannels: 1,
        pixelSpacing: image.pixelSpacing,
        ...(image.valueConversion ? { valueConversion: image.valueConversion } : {}),
        sha256: createHash('sha256').update(png).digest('hex'),
        transfer: pixelCount <= config.rawTransferMaxPixels ? 'raw' : 'server',
        // The window of the whole volume, so every slice opens alike
        windowMin: volume.info.windowMin,
        windowMax: volume.info.windowMax,
        histogram: image.histogram,
        warnings: volume.info.warnings,
        createdAt: new Date().toISOString(),
      };
      const pngPath = store.temporaryUploadPath();
      try {
        await fs.writeFile(pngPath, png);
        await store.save(info, image.pixels, pngPath);
      } finally {
        await fs.rm(pngPath, { force: true });
      }
      return reply.code(201).send(info);
    },
  );
};
