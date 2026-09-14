// Server-Sent Events over fetch (doc/ui-design-plan.md, section 8.2: EventSource cannot send the Authorization header
// that server mode needs).

import { apiFetch } from '../api/auth';

export interface SseMessage {
  event: string;
  data: string;
}

/** Incremental text/event-stream parser */
export class SseParser {
  private buffer = '';
  private event = '';
  private data: string[] = [];

  /** Adds text and returns the messages completed by it */
  push(text: string): SseMessage[] {
    this.buffer += text;
    const messages: SseMessage[] = [];
    let newline: number;
    while ((newline = this.buffer.search(/\r\n|\r|\n/)) >= 0) {
      const line = this.buffer.slice(0, newline);
      const length = this.buffer.startsWith('\r\n', newline) ? 2 : 1;
      this.buffer = this.buffer.slice(newline + length);
      this.line(line, messages);
    }
    return messages;
  }

  private line(line: string, messages: SseMessage[]): void {
    if (line === '') {
      if (this.data.length > 0) {
        messages.push({ event: this.event || 'message', data: this.data.join('\n') });
      }
      this.event = '';
      this.data = [];
      return;
    }
    if (line.startsWith(':')) {
      return; // comment, e.g. keep-alive
    }
    const colon = line.indexOf(':');
    const field = colon < 0 ? line : line.slice(0, colon);
    let value = colon < 0 ? '' : line.slice(colon + 1);
    if (value.startsWith(' ')) {
      value = value.slice(1);
    }
    if (field === 'event') {
      this.event = value;
    } else if (field === 'data') {
      this.data.push(value);
    }
  }
}

/** Reads an event stream until it ends or the signal aborts */
export async function readEventStream(url: string, onMessage: (message: SseMessage) => void, signal?: AbortSignal): Promise<void> {
  const response = await apiFetch(url, { signal, headers: { accept: 'text/event-stream' } });
  if (!response.ok || !response.body) {
    throw new Error(`Event stream failed with status ${response.status}`);
  }
  const parser = new SseParser();
  const reader = response.body.pipeThrough(new TextDecoderStream()).getReader();
  for (;;) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    for (const message of parser.push(value)) {
      onMessage(message);
    }
  }
}
