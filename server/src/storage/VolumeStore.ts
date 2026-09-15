import { randomUUID } from 'node:crypto';
import fs from 'node:fs/promises';
import path from 'node:path';
import type { VolumeInfo } from '@glcm/api';
import type { NativeStorage } from '@glcm/native';

const ID_PATTERN = /^vol_[0-9a-f]{32}$/;

export function newVolumeId(): string {
  return `vol_${randomUUID().replaceAll('-', '')}`;
}

export interface StoredVolume {
  info: VolumeInfo;
  /** How the slices are stored, passed back to extractNiftiSlice */
  storage: NativeStorage;
}

function isNotFound(error: unknown): boolean {
  return (error as NodeJS.ErrnoException | undefined)?.code === 'ENOENT';
}

/**
 * NIfTI volumes waiting for a slice to be chosen. Each volume has a folder volumes/<id>/ with
 * - volume.nii: the uploaded file, uncompressed, for fast access to any slice
 * - volume.json: StoredVolume
 *
 * Volumes are temporary: the web app deletes a volume once a slice is opened or the import is cancelled, a restart
 * removes them all, and retention deletes those left behind.
 */
export class VolumeStore {
  readonly volumesDir: string;

  constructor(dataDir: string) {
    this.volumesDir = path.join(dataDir, 'volumes');
  }

  async init(): Promise<void> {
    await fs.rm(this.volumesDir, { recursive: true, force: true });
    await fs.mkdir(this.volumesDir, { recursive: true });
  }

  private folder(id: string): string {
    if (!ID_PATTERN.test(id)) {
      throw new Error(`Invalid volume id: ${id}`);
    }
    return path.join(this.volumesDir, id);
  }

  /** Creates the folder of a new volume and returns the path for its uncompressed copy */
  async create(id: string): Promise<string> {
    await fs.mkdir(this.folder(id));
    return this.volumePath(id);
  }

  volumePath(id: string): string {
    return path.join(this.folder(id), 'volume.nii');
  }

  async save(volume: StoredVolume): Promise<void> {
    await fs.writeFile(path.join(this.folder(volume.info.volumeId), 'volume.json'), JSON.stringify(volume));
  }

  async get(id: string): Promise<StoredVolume | undefined> {
    try {
      return JSON.parse(await fs.readFile(path.join(this.folder(id), 'volume.json'), 'utf8')) as StoredVolume;
    } catch (error) {
      if (isNotFound(error)) {
        return undefined;
      }
      throw error;
    }
  }

  /** Every volume, oldest first */
  async list(): Promise<StoredVolume[]> {
    let names: string[];
    try {
      names = await fs.readdir(this.volumesDir);
    } catch (error) {
      if (isNotFound(error)) {
        return [];
      }
      throw error;
    }
    const volumes = await Promise.all(names.filter((name) => ID_PATTERN.test(name)).map((name) => this.get(name)));
    return volumes.filter((volume): volume is StoredVolume => volume !== undefined).sort((a, b) => a.info.createdAt.localeCompare(b.info.createdAt));
  }

  /** Removes a volume (also one whose upload failed half-way); false if it did not exist */
  async remove(id: string): Promise<boolean> {
    const folder = this.folder(id);
    try {
      await fs.access(folder);
    } catch {
      return false;
    }
    await fs.rm(folder, { recursive: true, force: true });
    return true;
  }
}
