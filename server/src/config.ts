import os from 'node:os';
import path from 'node:path';

export interface ServerConfig {
  host: string;
  port: number;
  /** Uploaded images and caches (doc/ui-design-plan.md, section 8.2) */
  dataDir: string;
  maxUploadBytes: number;
  maxImagePixels: number;
  /** Images up to this many pixels are sent to the browser as raw data (GET /raw) */
  rawTransferMaxPixels: number;
  /** Largest long side of display.png */
  displayMaxSize: number;
  /** Disk space for cached display.png renderings */
  displayCacheBytes: number;
  logLevel: string;
}

const MIB = 1024 * 1024;

export const DEFAULT_CONFIG: Omit<ServerConfig, 'dataDir'> = {
  host: '127.0.0.1',
  port: 8080,
  maxUploadBytes: 200 * MIB,
  maxImagePixels: 20_000 * 20_000,
  rawTransferMaxPixels: 4096 * 4096,
  displayMaxSize: 4096,
  displayCacheBytes: 512 * MIB,
  logLevel: 'info',
};

function integerSetting(env: NodeJS.ProcessEnv, name: string, fallback: number, minimum: number): number {
  const text = env[name];
  if (text === undefined || text === '') {
    return fallback;
  }
  const value = Number(text);
  if (!Number.isSafeInteger(value) || value < minimum) {
    throw new Error(`${name} must be an integer of at least ${minimum}, got "${text}"`);
  }
  return value;
}

/** Configuration from GLCM_* environment variables, falling back to DEFAULT_CONFIG */
export function loadConfig(env: NodeJS.ProcessEnv = process.env): ServerConfig {
  return {
    host: env.GLCM_HOST || DEFAULT_CONFIG.host,
    port: integerSetting(env, 'GLCM_PORT', DEFAULT_CONFIG.port, 0),
    dataDir: path.resolve(env.GLCM_DATA_DIR || path.join(os.homedir(), '.glcm-texture-analysis')),
    maxUploadBytes: integerSetting(env, 'GLCM_MAX_UPLOAD_BYTES', DEFAULT_CONFIG.maxUploadBytes, 1),
    maxImagePixels: integerSetting(env, 'GLCM_MAX_IMAGE_PIXELS', DEFAULT_CONFIG.maxImagePixels, 1),
    rawTransferMaxPixels: integerSetting(env, 'GLCM_RAW_TRANSFER_MAX_PIXELS', DEFAULT_CONFIG.rawTransferMaxPixels, 0),
    displayMaxSize: integerSetting(env, 'GLCM_DISPLAY_MAX_SIZE', DEFAULT_CONFIG.displayMaxSize, 1),
    displayCacheBytes: integerSetting(env, 'GLCM_DISPLAY_CACHE_BYTES', DEFAULT_CONFIG.displayCacheBytes, 0),
    logLevel: env.GLCM_LOG_LEVEL || DEFAULT_CONFIG.logLevel,
  };
}

export function isLoopbackHost(host: string): boolean {
  return host === '127.0.0.1' || host === '::1' || host === 'localhost';
}
