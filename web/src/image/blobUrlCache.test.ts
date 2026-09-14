import { describe, expect, it } from 'vitest';
import { BlobUrlCache } from './blobUrlCache';

function createCache(capacity: number) {
  let counter = 0;
  const revoked: string[] = [];
  const cache = new BlobUrlCache(
    capacity,
    () => `blob:${(counter += 1)}`,
    (url) => revoked.push(url),
  );
  return { cache, revoked };
}

const blob = new Blob(['png']);

describe('BlobUrlCache', () => {
  it('evicts and revokes the least recently used URL', () => {
    const { cache, revoked } = createCache(2);
    expect(cache.set('a', blob)).toBe('blob:1');
    cache.set('b', blob);
    expect(cache.get('a')).toBe('blob:1'); // a is now the most recent
    cache.set('c', blob);
    expect(revoked).toEqual(['blob:2']);
    expect(cache.get('b')).toBeUndefined();
    expect(cache.size).toBe(2);
  });

  it('revokes replaced and cleared URLs', () => {
    const { cache, revoked } = createCache(20);
    cache.set('a', blob);
    expect(cache.set('a', blob)).toBe('blob:2');
    expect(revoked).toEqual(['blob:1']);
    cache.clear();
    expect(revoked).toEqual(['blob:1', 'blob:2']);
    expect(cache.size).toBe(0);
  });
});
