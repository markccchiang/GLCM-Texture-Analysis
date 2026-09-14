// Access token of server mode (doc/ui-design-plan.md, section 8.2): asked once, kept in sessionStorage (closing the tab
// forgets it) and sent as "Authorization: Bearer <token>" with every API request. Images and progress events are
// fetched with the header too, which <img> and EventSource could not do.

import { create } from 'zustand';

const STORAGE_KEY = 'glcm.apiToken';

function readStoredToken(): string | null {
  try {
    return window.sessionStorage.getItem(STORAGE_KEY);
  } catch {
    return null;
  }
}

function storeToken(token: string | null): void {
  try {
    if (token) {
      window.sessionStorage.setItem(STORAGE_KEY, token);
    } else {
      window.sessionStorage.removeItem(STORAGE_KEY);
    }
  } catch {
    // Storage blocked: the token lasts until the page is reloaded
  }
}

export interface AuthState {
  token: string | null;
  /** The token prompt is shown */
  promptOpen: boolean;
  setToken(token: string): void;
  /** Forgets the token and asks for one, e.g. after a 401 */
  requireToken(): void;
}

export const useAuth = create<AuthState>()((set) => ({
  token: readStoredToken(),
  promptOpen: false,
  setToken: (token) => {
    storeToken(token);
    set({ token, promptOpen: false });
  },
  requireToken: () => {
    storeToken(null);
    set({ token: null, promptOpen: true });
  },
}));

export function authHeaders(): Record<string, string> {
  const { token } = useAuth.getState();
  return token ? { authorization: `Bearer ${token}` } : {};
}

/** fetch with the access token; a 401 response asks the user for a token */
export async function apiFetch(url: string, init: RequestInit = {}): Promise<Response> {
  const headers = new Headers(init.headers);
  for (const [name, value] of Object.entries(authHeaders())) {
    headers.set(name, value);
  }
  const response = await fetch(url, { ...init, headers });
  if (response.status === 401) {
    useAuth.getState().requireToken();
  }
  return response;
}
