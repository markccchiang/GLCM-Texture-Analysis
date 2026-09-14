import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { apiFetch, authHeaders, useAuth } from './auth';

const TOKEN = 'a'.repeat(44);

beforeEach(() => {
  window.sessionStorage.clear();
  useAuth.setState({ token: null, promptOpen: false });
});

afterEach(() => {
  vi.unstubAllGlobals();
});

describe('access token', () => {
  it('keeps the token in session storage and sends it', async () => {
    useAuth.getState().setToken(TOKEN);
    expect(window.sessionStorage.getItem('glcm.apiToken')).toBe(TOKEN);
    expect(authHeaders()).toEqual({ authorization: `Bearer ${TOKEN}` });

    const fetchMock = vi.fn(async () => new Response('{}', { status: 200 }));
    vi.stubGlobal('fetch', fetchMock);
    await apiFetch('/api/v1/catalog', { headers: { accept: 'application/json' } });
    const [, init] = fetchMock.mock.calls[0] as unknown as [string, RequestInit];
    const headers = new Headers(init.headers);
    expect(headers.get('authorization')).toBe(`Bearer ${TOKEN}`);
    expect(headers.get('accept')).toBe('application/json');
  });

  it('sends no header without a token', async () => {
    const fetchMock = vi.fn(async () => new Response('{}', { status: 200 }));
    vi.stubGlobal('fetch', fetchMock);
    await apiFetch('/api/v1/catalog');
    const [, init] = fetchMock.mock.calls[0] as unknown as [string, RequestInit];
    expect(new Headers(init.headers).has('authorization')).toBe(false);
    expect(useAuth.getState().promptOpen).toBe(false);
  });

  it('forgets a rejected token and asks for one', async () => {
    useAuth.getState().setToken(TOKEN);
    vi.stubGlobal('fetch', vi.fn(async () => new Response('{"error": "Unauthorized"}', { status: 401 })));
    const response = await apiFetch('/api/v1/catalog');
    expect(response.status).toBe(401);
    expect(useAuth.getState()).toMatchObject({ token: null, promptOpen: true });
    expect(window.sessionStorage.getItem('glcm.apiToken')).toBeNull();
  });
});
