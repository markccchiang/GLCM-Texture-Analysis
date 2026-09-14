import { randomUUID } from 'node:crypto';
import fs from 'node:fs/promises';
import path from 'node:path';
import type { ImageInfo } from '@glcm/api';
import { compress, type ContentEncoding } from '../encoding.js';

const ID_PATTERN = /^img_[0-9a-f]{32}$/;

export function newImageId(): string {
  return `img_${randomUUID().replaceAll('-', '')}`;
}

function isNotFound(error: unknown): boolean {
  return (error as NodeJS.ErrnoException | undefined)?.code === 'ENOENT';
}

interface CachedPixels {
  pixels: Promise<Buffer>;
  /** Size once read; 0 while reading */
  bytes: number;
}

/**
 * Images on local disk (doc/ui-design-plan.md, section 8.2). Each image has a folder images/<id>/ with
 * - original: the uploaded file
 * - pixels.bin: row-major grayscale samples, 16-bit samples little-endian
 * - pixels.bin.<gzip|zstd>: compressed copies, created on first request
 * - info.json: ImageInfo
 *
 * Pixel buffers of recently used images are kept in memory, up to pixelCacheBytes, so ROI statistics requests sent
 * while ROIs are edited do not read the whole file each time. Requests for the same image share one read.
 */
export class ImageStore {
  readonly imagesDir: string;
  readonly uploadsDir: string;
  private readonly pendingCompressions = new Map<string, Promise<Buffer>>();
  /** Least recently used first */
  private readonly cachedPixels = new Map<string, CachedPixels>();
  private cachedBytes = 0;

  /** pixelCacheBytes 0 disables the pixel cache */
  constructor(
    dataDir: string,
    private readonly pixelCacheBytes = 0,
  ) {
    this.imagesDir = path.join(dataDir, 'images');
    this.uploadsDir = path.join(dataDir, 'uploads');
  }

  async init(): Promise<void> {
    await fs.mkdir(this.imagesDir, { recursive: true });
    // Leftovers of interrupted uploads
    await fs.rm(this.uploadsDir, { recursive: true, force: true });
    await fs.mkdir(this.uploadsDir, { recursive: true });
  }

  /** Path for streaming an upload before it is decoded */
  temporaryUploadPath(): string {
    return path.join(this.uploadsDir, `${randomUUID()}.upload`);
  }

  private folder(id: string): string {
    if (!ID_PATTERN.test(id)) {
      throw new Error(`Invalid image id: ${id}`);
    }
    return path.join(this.imagesDir, id);
  }

  async save(info: ImageInfo, pixels: Buffer, uploadPath: string): Promise<void> {
    const folder = this.folder(info.imageId);
    await fs.mkdir(folder);
    await fs.rename(uploadPath, path.join(folder, 'original'));
    await fs.writeFile(path.join(folder, 'pixels.bin'), pixels);
    await fs.writeFile(path.join(folder, 'info.json'), JSON.stringify(info));
  }

  async info(id: string): Promise<ImageInfo | undefined> {
    try {
      return JSON.parse(await fs.readFile(path.join(this.folder(id), 'info.json'), 'utf8')) as ImageInfo;
    } catch (error) {
      if (isNotFound(error)) {
        return undefined;
      }
      throw error;
    }
  }

  /** pixels.bin, possibly shared with other requests: callers must not modify the buffer */
  pixels(id: string): Promise<Buffer> {
    const file = path.join(this.folder(id), 'pixels.bin');
    if (this.pixelCacheBytes === 0) {
      return fs.readFile(file);
    }

    const cached = this.cachedPixels.get(id);
    if (cached) {
      this.cachedPixels.delete(id);
      this.cachedPixels.set(id, cached);
      return cached.pixels;
    }

    const entry: CachedPixels = { pixels: fs.readFile(file), bytes: 0 };
    this.cachedPixels.set(id, entry);
    entry.pixels.then(
      (pixels) => {
        // The image may have been removed while reading
        if (this.cachedPixels.get(id) === entry) {
          entry.bytes = pixels.length;
          this.cachedBytes += pixels.length;
          this.evictPixels();
        }
      },
      () => {
        if (this.cachedPixels.get(id) === entry) {
          this.cachedPixels.delete(id);
        }
      },
    );
    return entry.pixels;
  }

  /** Bytes of pixel buffers held in memory */
  get pixelCacheSize(): number {
    return this.cachedBytes;
  }

  private evictPixels(): void {
    for (const [id, entry] of this.cachedPixels) {
      if (this.cachedBytes <= this.pixelCacheBytes) {
        break;
      }
      // Reads still in progress are not counted yet
      if (entry.bytes > 0) {
        this.forgetPixels(id);
      }
    }
  }

  private forgetPixels(id: string): void {
    const entry = this.cachedPixels.get(id);
    if (entry) {
      this.cachedBytes -= entry.bytes;
      this.cachedPixels.delete(id);
    }
  }

  /** One sample, read from pixels.bin without loading the whole image */
  async pixelValue(info: ImageInfo, x: number, y: number): Promise<number> {
    const bytesPerSample = info.bitDepth / 8;
    const buffer = Buffer.alloc(bytesPerSample);
    const handle = await fs.open(path.join(this.folder(info.imageId), 'pixels.bin'), 'r');
    try {
      await handle.read(buffer, 0, bytesPerSample, (y * info.width + x) * bytesPerSample);
    } finally {
      await handle.close();
    }
    return bytesPerSample === 2 ? buffer.readUInt16LE(0) : buffer.readUInt8(0);
  }

  /** pixels.bin in the given encoding; compressed copies are created once and reused */
  async encodedPixels(id: string, encoding: ContentEncoding): Promise<Buffer> {
    const source = path.join(this.folder(id), 'pixels.bin');
    if (encoding === 'identity') {
      return this.pixels(id);
    }

    const target = `${source}.${encoding}`;
    try {
      return await fs.readFile(target);
    } catch (error) {
      if (!isNotFound(error)) {
        throw error;
      }
    }

    let pending = this.pendingCompressions.get(target);
    if (!pending) {
      pending = (async () => {
        const compressed = await compress(await this.pixels(id), encoding);
        const temporary = `${target}.${randomUUID()}.tmp`;
        await fs.writeFile(temporary, compressed);
        await fs.rename(temporary, target);
        return compressed;
      })().finally(() => this.pendingCompressions.delete(target));
      this.pendingCompressions.set(target, pending);
    }
    return pending;
  }

  /** Path of the uploaded file */
  originalPath(id: string): string {
    return path.join(this.folder(id), 'original');
  }

  /** Every stored image, newest first */
  async list(): Promise<ImageInfo[]> {
    let names: string[];
    try {
      names = await fs.readdir(this.imagesDir);
    } catch (error) {
      if (isNotFound(error)) {
        return [];
      }
      throw error;
    }
    const infos = await Promise.all(names.filter((name) => ID_PATTERN.test(name)).map((name) => this.info(name)));
    return infos.filter((info): info is ImageInfo => info !== undefined).sort((a, b) => b.createdAt.localeCompare(a.createdAt));
  }

  /** Removes an image; false if it did not exist */
  async remove(id: string): Promise<boolean> {
    const folder = this.folder(id);
    this.forgetPixels(id);
    try {
      await fs.access(folder);
    } catch {
      return false;
    }
    await fs.rm(folder, { recursive: true, force: true });
    return true;
  }
}
