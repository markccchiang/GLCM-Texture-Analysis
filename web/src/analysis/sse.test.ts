import { describe, expect, it } from 'vitest';
import { SseParser } from './sse';

describe('SseParser', () => {
  it('parses events split across chunks', () => {
    const parser = new SseParser();
    expect(parser.push('event: progress\ndata: {"completed"')).toEqual([]);
    expect(parser.push(': 1}\n')).toEqual([]);
    expect(parser.push('\nevent: finished\r\ndata: {}\r\n\r\n')).toEqual([
      { event: 'progress', data: '{"completed": 1}' },
      { event: 'finished', data: '{}' },
    ]);
  });

  it('ignores comments, joins data lines and defaults the event name', () => {
    const parser = new SseParser();
    expect(parser.push(': keep-alive\n\ndata: a\ndata:b\n\nevent: empty\n\n')).toEqual([{ event: 'message', data: 'a\nb' }]);
  });
});
