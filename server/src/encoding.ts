import zlib from 'node:zlib';

export type ContentEncoding = 'zstd' | 'gzip' | 'identity';

/** zstd is built into node:zlib from Node.js 22.15 / 23.8 */
export const ZSTD_AVAILABLE = typeof zlib.zstdCompress === 'function';

/**
 * Encoding for a response body from the Accept-Encoding header: zstd when the client accepts it and Node.js supports
 * it, otherwise gzip when accepted, otherwise identity. q=0 excludes an encoding; "*" matches any.
 */
export function negotiateEncoding(acceptEncoding: string | string[] | undefined): ContentEncoding {
  const header = Array.isArray(acceptEncoding) ? acceptEncoding.join(',') : (acceptEncoding ?? '');
  const quality = new Map<string, number>();
  for (const part of header.split(',')) {
    const [name, ...parameters] = part.trim().toLowerCase().split(';');
    if (!name) {
      continue;
    }
    let q = 1;
    for (const parameter of parameters) {
      const [key, value] = parameter.trim().split('=');
      if (key === 'q' && value !== undefined) {
        q = Number(value);
      }
    }
    quality.set(name, Number.isFinite(q) ? q : 0);
  }

  const accepts = (name: string) => (quality.get(name) ?? quality.get('*') ?? 0) > 0;
  if (ZSTD_AVAILABLE && accepts('zstd')) {
    return 'zstd';
  }
  if (accepts('gzip')) {
    return 'gzip';
  }
  return 'identity';
}

export function compress(data: Buffer, encoding: Exclude<ContentEncoding, 'identity'>): Promise<Buffer> {
  return new Promise((resolve, reject) => {
    const done = (error: Error | null, result: Buffer) => (error ? reject(error) : resolve(result));
    if (encoding === 'gzip') {
      zlib.gzip(data, { level: 6 }, done);
    } else if (ZSTD_AVAILABLE) {
      // Bodies are compressed once and stored, so a higher level than zstd's default (3) is worth it
      zlib.zstdCompress(data, { params: { [zlib.constants.ZSTD_c_compressionLevel]: 12 } }, done);
    } else {
      reject(new Error('zstd is not available in this Node.js version'));
    }
  });
}
