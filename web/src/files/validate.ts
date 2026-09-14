// Reading JSON files written by the app, validated with the shared schemas (doc/ui-design-plan.md, section 8.4: every
// file has a format and an integer version, and readers reject versions they don't know).

import type { TSchema } from 'typebox';
import { Value } from 'typebox/value';

export class FileFormatError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'FileFormatError';
  }
}

export function parseAppFile<T>(text: string, schema: TSchema, format: string, label: string): T {
  let value: unknown;
  try {
    value = JSON.parse(text);
  } catch {
    throw new FileFormatError(`The ${label} is not valid JSON.`);
  }
  if (typeof value !== 'object' || value === null || Array.isArray(value)) {
    throw new FileFormatError(`The ${label} must contain a JSON object.`);
  }
  const document = value as { format?: unknown; version?: unknown };
  if (document.format !== format) {
    throw new FileFormatError(`This is not a ${label}: its format is ${JSON.stringify(document.format ?? null)}, expected "${format}".`);
  }
  if (document.version !== 1) {
    throw new FileFormatError(`The ${label} has version ${JSON.stringify(document.version ?? null)}; only version 1 is supported.`);
  }
  if (!Value.Check(schema, value)) {
    const [first] = [...Value.Errors(schema, value)] as Array<{ instancePath?: string; message?: string }>;
    const where = first?.instancePath ? first.instancePath.replace(/^\//, '').replaceAll('/', '.') : 'document';
    throw new FileFormatError(`The ${label} is invalid: ${where} ${first?.message ?? 'does not match the format'}.`);
  }
  return value as T;
}
