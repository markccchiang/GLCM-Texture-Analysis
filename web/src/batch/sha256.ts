/**
 * Hex SHA-256 of a file, like the server computes for uploads; null where the browser offers no digest (Web Crypto
 * needs a secure context: HTTPS or localhost), in which case the image is simply uploaded again
 */
export async function fileSha256(file: Blob): Promise<string | null> {
  if (!globalThis.crypto?.subtle) {
    return null;
  }
  const digest = await globalThis.crypto.subtle.digest('SHA-256', await file.arrayBuffer());
  return [...new Uint8Array(digest)].map((byte) => byte.toString(16).padStart(2, '0')).join('');
}
