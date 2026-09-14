// Saving files from the browser.

/** Offers a blob as a download */
export function downloadBlob(blob: Blob, fileName: string): void {
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = fileName;
  link.style.display = 'none';
  document.body.append(link);
  link.click();
  link.remove();
  // Revoked later: some browsers start reading the URL only after click() returns
  window.setTimeout(() => URL.revokeObjectURL(url), 30_000);
}

export function downloadText(text: string, fileName: string, type = 'application/json'): void {
  downloadBlob(new Blob([text], { type: `${type};charset=utf-8` }), fileName);
}

/** File name from a Content-Disposition header (RFC 6266), preferring the UTF-8 form */
export function fileNameFromDisposition(header: string | null, fallback: string): string {
  if (!header) {
    return fallback;
  }
  const extended = /filename\*=UTF-8''([^;]+)/i.exec(header);
  if (extended) {
    try {
      return decodeURIComponent(extended[1]);
    } catch {
      // Malformed encoding: try the plain name
    }
  }
  const plain = /filename="([^"]*)"/i.exec(header) ?? /filename=([^;]+)/i.exec(header);
  return plain?.[1]?.trim() || fallback;
}

/** File name without directories and extension, restricted to safe characters (as the server does) */
export function fileStem(fileName: string): string {
  const base = fileName.split(/[\\/]/).pop() ?? '';
  const stem = base.includes('.') ? base.slice(0, base.lastIndexOf('.')) : base;
  return stem.replace(/[^A-Za-z0-9._-]+/g, '_').replace(/^\.+/, '').slice(0, 100) || 'image';
}

/** Local time as YYYYMMDD-HHMMSS for file names */
export function timestampForFileName(date = new Date()): string {
  const pad = (value: number) => String(value).padStart(2, '0');
  return `${date.getFullYear()}${pad(date.getMonth() + 1)}${pad(date.getDate())}-${pad(date.getHours())}${pad(date.getMinutes())}${pad(date.getSeconds())}`;
}
