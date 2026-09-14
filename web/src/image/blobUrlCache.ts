// In-memory LRU of object URLs for display.png renderings (doc/ui-design-plan.md, section 6.1, Caching).

export class BlobUrlCache {
  private readonly entries = new Map<string, string>();

  constructor(
    readonly capacity = 20,
    private readonly createUrl: (blob: Blob) => string = (blob) => URL.createObjectURL(blob),
    private readonly revokeUrl: (url: string) => void = (url) => URL.revokeObjectURL(url),
  ) {}

  get size(): number {
    return this.entries.size;
  }

  /** The URL for a key, marking it as most recently used */
  get(key: string): string | undefined {
    const url = this.entries.get(key);
    if (url !== undefined) {
      this.entries.delete(key);
      this.entries.set(key, url);
    }
    return url;
  }

  /** Stores a blob and returns its URL; the least recently used entries beyond the capacity are revoked */
  set(key: string, blob: Blob): string {
    const previous = this.entries.get(key);
    if (previous !== undefined) {
      this.entries.delete(key);
      this.revokeUrl(previous);
    }
    const url = this.createUrl(blob);
    this.entries.set(key, url);
    while (this.entries.size > this.capacity) {
      const [oldestKey, oldestUrl] = this.entries.entries().next().value as [string, string];
      this.entries.delete(oldestKey);
      this.revokeUrl(oldestUrl);
    }
    return url;
  }

  clear(): void {
    for (const url of this.entries.values()) {
      this.revokeUrl(url);
    }
    this.entries.clear();
  }
}
