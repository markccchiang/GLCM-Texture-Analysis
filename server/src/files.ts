// Download helpers: Content-Disposition headers and ZIP archives.

import path from 'node:path';
import { zipSync, type Zippable } from 'fflate';

export interface NamedFile {
  name: string;
  data: Uint8Array;
}

/** Content-Disposition for a download, with an ASCII fallback name and the UTF-8 name (RFC 6266) */
export function attachment(fileName: string): string {
  const fallback = fileName.replace(/[^\x20-\x7e]|["\\]/g, '_');
  return `attachment; filename="${fallback}"; filename*=UTF-8''${encodeURIComponent(fileName)}`;
}

/** File name without extension, restricted to safe characters */
export function fileStem(fileName: string): string {
  const stem = path.parse(path.basename(fileName)).name.replace(/[^A-Za-z0-9._-]+/g, '_').replace(/^\.+/, '');
  return stem.slice(0, 100) || 'image';
}

// Already compressed formats are stored as they are
const STORED = /\.(png|jpe?g|zip)$/i;

export function createZip(files: readonly NamedFile[]): Buffer {
  const entries: Zippable = {};
  for (const file of files) {
    entries[file.name] = [file.data, { level: STORED.test(file.name) ? 0 : 6 }];
  }
  return Buffer.from(zipSync(entries));
}
