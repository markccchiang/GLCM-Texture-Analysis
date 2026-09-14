import type { CatalogResponse } from '@glcm/api';
import { QueryClient } from '@tanstack/react-query';
import { getCatalog } from './client';

export const queryClient = new QueryClient({
  defaultOptions: { queries: { staleTime: 60_000, retry: 1, refetchOnWindowFocus: false } },
});

export const CATALOG_QUERY = { queryKey: ['catalog'], queryFn: ({ signal }: { signal: AbortSignal }) => getCatalog(signal), staleTime: Infinity };

/** The feature catalog, from the cache when available */
export function loadCatalog(): Promise<CatalogResponse> {
  return queryClient.fetchQuery(CATALOG_QUERY);
}
