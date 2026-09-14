import fs from 'node:fs/promises';
import path from 'node:path';

/**
 * Size-capped least-recently-used cache of rendered display.png files on disk. Keys are hex digests of the image hash
 * and rendering parameters, so entries never go stale; the cache is emptied when the server starts.
 */
export class DisplayCache {
  private readonly entries = new Map<string, number>(); // key -> size in bytes; Map order is least recent first
  private totalBytes = 0;

  constructor(
    private readonly directory: string,
    private readonly maxBytes: number,
  ) {}

  async init(): Promise<void> {
    await fs.rm(this.directory, { recursive: true, force: true });
    await fs.mkdir(this.directory, { recursive: true });
  }

  get sizeBytes(): number {
    return this.totalBytes;
  }

  private file(key: string): string {
    if (!/^[0-9a-f]{64}$/.test(key)) {
      throw new Error(`Invalid cache key: ${key}`);
    }
    return path.join(this.directory, `${key}.png`);
  }

  async get(key: string): Promise<Buffer | undefined> {
    const size = this.entries.get(key);
    if (size === undefined) {
      return undefined;
    }
    try {
      const data = await fs.readFile(this.file(key));
      this.entries.delete(key);
      this.entries.set(key, size);
      return data;
    } catch {
      this.forget(key);
      return undefined;
    }
  }

  async set(key: string, data: Buffer): Promise<void> {
    if (data.length > this.maxBytes) {
      return;
    }
    await fs.writeFile(this.file(key), data);
    this.forget(key);
    this.entries.set(key, data.length);
    this.totalBytes += data.length;

    while (this.totalBytes > this.maxBytes) {
      const oldest = this.entries.keys().next().value as string;
      this.forget(oldest);
      await fs.rm(this.file(oldest), { force: true });
    }
  }

  private forget(key: string): void {
    const size = this.entries.get(key);
    if (size !== undefined) {
      this.entries.delete(key);
      this.totalBytes -= size;
    }
  }
}
